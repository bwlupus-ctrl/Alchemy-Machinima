# Adversarial review R2: DIRECTOR_KIT_P0_SPEC.md, DIRECTOR_KIT_ROLLOUT_SPEC.md, VCAM_MONITORS_DESIGN.md

Reviewer: Opus (independent context), 2026-10-10. Inputs: `git diff -- doc` (P0 spec +272/−152, VCam doc +135) and the new
`doc/DIRECTOR_KIT_ROLLOUT_SPEC.md` (267 lines). Every revised code anchor was re-checked against the tree, and the rollout
matrix was cross-checked against the data snapshot. For each page I resolved which XML file really hosts each data widget
and which `.cpp` files reference it by name, using a scan of `skins/default/xui/en/*.xml` and `indra/newview/*.cpp`.

**VERDICT: NO-GO.**
- The P0 spec is now close to GO-WITH-FIXES. Most R1 findings there are genuinely fixed and the new anchors are accurate.
- The revision created one new blocker in the Light map (live display combined with relative drag runs away).
- The rollout spec is not implementable without guessing:
  - its "v1 source" column points at Director launcher cards for about a third of the pages, instead of the floaters and panels that actually own the controls;
  - it classifies heavily C++-driven pages as "pure-setting";
  - it relies on custom widget kinds (path canvas, curve editor) that exist nowhere in the tree and have no behavioural spec.

Counts: **3 BLOCKER, 12 MAJOR, 6 MINOR.** R1 status: 25 resolved, 7 partial, 3 superseded by new findings, 1 unresolved (table at the end).

---

## BLOCKER

### N1. Light map: live display plus relative Top-view drag runs away (new in R2).
- **Doc:** P0 §3.14 Frames, Display transform, Input ("display source is `selected().lastFrame().mProj[i].mOff*`"; "Top-view drag: `Δ=wrap180(t_new−t_cur)`, live `base_yaw += mirror ? −Δ : Δ`").
- **Evidence:**
  - `lastFrame` is rendered from `mCurrentLive` (`alcinelightrig.cpp:2961`).
  - `mCurrentLive` eases toward the newly computed target over `globals.mTransitionSec` (`:1937-1958`, `:2954-2956`). `CineLightRigTransitionSec` defaults to **0.9 s** with ease-in-out. FX setups also animate it (`:2925`).
  - While the user drags, `t_cur` (the live display) lags the setting by up to 0.9 s. Every hover event re-adds the whole remaining gap `t_new − t_cur` to `base_yaw`, so the light overshoots and spins.
  - The live vertical inverse has the same flaw. `baseFromDisplayPitch(target, orbit, h, r, current)` is fed `r = CineLightRigRadius`, but the live `p_disp` comes from `effective_radius` (subject-scaled, `alcinelightrigmodel.cpp:1615-1621`), so a scaled subject jumps at drag start.
- **Why it breaks:** dragging a light is the map's core interaction. With the default settings it is unusable.
- **Fix:**
  - Drags are **absolute from the gesture start**: `base_new = base0 + sign·(t_pointer − t_pointer0)`, with `t_pointer0` captured on mouse-down. Never derive Δ from the displayed live position after mouse-down.
  - During a drag, draw the dragged light at its **target** position (computed from settings, the fallback model scaled by the live effective radius), not the eased live position.
  - The pitch inverse uses the same radius as the display. Use `effective_radius = hypot(ox, oy, oz − h)`-consistent values when live.
  - TUT: 60 synthetic hover events against a display that lags by 0.9 s must end within 0.5° of the pointer.

### N2. The rollout matrix names the wrong source and owner for at least 15 of 36 pages, and misclassifies C++-driven pages as pure-setting.
- **Doc:** ROLLOUT §4 matrix, §2 (pure-setting definition), §5 parity ("every v1 leaf and recursively expanded `filename=` source").
- **Evidence:** a scan of the data widgets against the real XML hosts and C++ references.

  **Lightbox-hosted pages.** The v1 Director leaf is only a launcher card (`floater_director.xml:1402-1408`, `1421-1424`, `1444-1450`, `1455-1466`, `1711-1717`). The real controls live in `floater_lightbox_settings.xml` and are driven by `alfloaterlightbox.cpp` (1484 lines, 91 lookups):
  | Page | Rows hosted in the Lightbox | Rows the C++ references by name |
  |---|---|---|
  | Tron | 130 | 96 |
  | Roto Ink | 79 | 30 |
  | Image | 21 | 15 (53 of its rows have names found nowhere) |
  | Shadows & Probes | 36 | — |
  | Practicals | 15 | 15 |
  | Atmosphere | 15 | — |
  | Actors | 5 | — |

  For Tron that C++ is real behaviour, not "seven named actions": palette apply, the Custom fallback, grid-origin snap, Roto and Rig Rim integration presets, and a 30-widget visibility gate (`alfloaterlightbox.cpp:839-929`). The matrix calls Tron, Roto, Image and Shadows "pure-setting" and cites the launcher-card lines as their v1 source. Parity will never see these widgets or their C++, because they are not in a v1 leaf.

  **Floater-hosted pages.** The rollout does not name the owning classes:
  - Light Cues → `floater_cine_light_cues.xml` / `ALFloaterCineLightCues` (413 lines);
  - Gaze Cues → `floater_gaze_cues.xml` / `ALFloaterGazeCues` (686 lines). Its 3 data widgets exist in no XML at all;
  - Diopter → `floater_ultimate_diopter.xml` / `ALFloaterUltimateDiopter` (1589 lines);
  - Capture → the Snapshot floater, described only as "a capture settings registry", which does not exist.

  **Logic owned by `LLFloaterDirector`.** These pages' widget references live in `llfloaterdirector.cpp`, which no WP7–WP11 file list includes:
  - Animate: 16 of 24 (the rollout says "over `ALPanelAnimPreview`", a 332-line preview panel);
  - Actor FX: 17 of 18 (`refreshActorStyleTab`, `llfloaterdirector.cpp:2458`);
  - Move: 8;
  - Frame & Orbit: 8, plus `llviewerjoystick.cpp` 7 and `llcinematiccamera.cpp` 7. The rollout calls it pure-setting.

  **Other mismatches:**
  - Operator: 66 of 69 widgets are referenced in `alpanelcinecamparams.cpp`, yet it is called "pure-setting".
  - Shot: 254 rows are in `panel_cinecam_params.xml`, with 259 name references in `llcinematiccamera.cpp`. The matrix gives one sentence.
  - Takes: 5 rows resolve to `floater_joystick.xml` and 2 to `floater_env_intensity_presets.xml`. Either these are name collisions in the data or undeclared sources; the matrix lists only `panel_flycam_recorder.xml`.
- **Why it breaks:** under the pure-setting rule, Codex will emit plain setting rows. The C++ that populates combos, applies presets, gates visibility and syncs state never runs for the Director copies. The behaviour is silently dropped, and the parity gate passes because its v1 source set is wrong.
- **Fix:** replace matrix columns 3–4 with generated, verifiable data, a `page_sources.json` produced by a script.
  - For each page: every XML file hosting its data widgets (resolving name collisions explicitly); every `.cpp` referencing them; and the owning class.
  - The classification follows mechanically: any row whose widget is referenced from C++ makes the page code-driven, with a named controller extracted from that `.cpp`.
  - Parity's source set becomes this file, not "v1 leaf + filename embeds".
  - WP7/WP8/WP10 file lists add `llfloaterdirector.*` (Move, Animate, Actor FX logic), `alfloaterlightbox.*`, `alfloatercinelightcues.*`, `alfloatergazecues.*`, `alfloaterultimatediopter.*` and the Snapshot owner.

### N3 (B1 residual). Rollout custom kinds have no backing implementation and no behavioural spec.
- **Doc:** ROLLOUT §3 (`path_canvas`, `curve_editor`, `slot_grid`, `asset_picker`, `preview_view`) and §4 assignments (`curve_editor` for Gaze Cues, Shot, Takes, Exposure, Light Cues; `path_canvas` for Path).
- **Evidence:**
  - No `ALPathCanvas`, curve, timeline, keyframe or slot-grid class exists in `indra/newview` or `indra/llui`. A grep for `class \w*(Curve|Timeline|Keyframe|SlotGrid|Canvas)` finds only an enum in `aldanceballmodel.h`.
  - The real sources contain only stock widgets. A tag census of `panel_path_editor.xml`, `floater_gaze_cues.xml`, `floater_cine_light_cues.xml`, `panel_exposure_hub.xml`, `panel_flycam_recorder.xml`, `panel_director_switcher.xml` and `panel_director_anim_switcher.xml` shows only button, combo_box, spinner, scroll_list, text, slider, check_box and accordion. The cue stacks are lists, not curves.
  - The rollout specifies these kinds' **layout** only (height clamps, audit states). It gives no data model, no interactions, no binding to which model fields, and no undo granularity.
- **Why it breaks:** Codex must invent five new editors. Each needs its model contract, input semantics and typed-history entries, none of which exist. For Path in particular, an "editor canvas" has no counterpart in v1.
- **Fix:** either (a) drop the invented kinds where v1 is a list (cue stacks become `list` rows plus per-cue kit rows; Path becomes its node list plus kit rows), or (b) add a behavioural sub-spec per kind. That sub-spec covers: the model accessor (exact class::method), the coordinate mapping, pointer, wheel and key interactions, the typed `ALKitHistory` entries, and the audit-state enumerator. Ground each one in a mockup screen and a named existing model.

---

## MAJOR

### M-R1. B2 residual: programmatic `setValue` on a kit switch is not a flip source, so state text, amber and mode cards go stale.
- **Doc:** P0 §3.2 ("`setValue()` … is not the source of flip detection").
- **Evidence:** C++ drives several switches with the wrapper's `setValue`, which neither the control signal nor `onCommit` sees:
  - `mGroupEnable->setValue` and the group slot checks (`alpanelcinelightrig.cpp:1630, 1634`);
  - `setValueIfChanged(mShaftControls/mHeroControls…)` (`:2583-2584`);
  - most importantly `setValueIfChanged(mEasyModeToggle, LLSD(mEasyModeActive), true)` (`:2390`). That call deliberately shows the **per-instance** mode, which can differ from the `CineLightRigEasyMode` setting when `normalizeSelectedForEasy` refuses (`:2230-2235`). `setValueIfChanged` calls the LLSD `setValue` and never commits (`aluirefresh.h:291-302`).
- **Why it breaks:** dimming is fine, because it reads `isOn()` per frame. But the "On/Off" state text, amber, and **the page's Easy/Advanced card set** only change on a flip. A Rig instance kept in Advanced then shows Easy cards with dead controls, which is the R1 B3 symptom again.
- **Fix:** the `setValue` override also calls `noteState(value.asBoolean())`; `noteState` is edge-only, so there is no double fire. The page's mode is driven by `applyEasyMode`'s result, published through the mode adapter, not by the switch alone.

### M-R2. B3 residual: the `applyEasyMode` contract is ambiguous on re-entrancy and on which panel instance it targets.
- **Doc:** P0 §3.5 and §3.18 (a "stateless mode adapter … resolves the live Rig panel handle").
- **Evidence:**
  - Two `ALPanelCineLightRig` instances can be live at once: the standalone floater and the Director page. Both are in `LIVE_PANELS` (`alpanelcinelightrig.cpp:823-827`).
  - `applyEasyMode` writes `CineLightRigEasyMode` (as `onEasyModeCommit` does at `:2226`). The spec also routes "setting-signal changes" into `applyEasyMode`, so a call re-enters itself through the setting signal.
- **Fix:** the adapter is resolved per page: the page's own logic-root handle, never a global "live panel". Add a re-entrancy guard (reuse `mSyncingEasyControls`). Say explicitly that the standalone floater's instance reacts to the setting signal independently and must not write the setting back.

### M-R3. B4 residual: VCam undo of Remove cannot restore the Gate.
- **Doc:** VCAM §5.4 (`RemovedCaptureRecord` "capture, owned displays … Gate runtime untouched").
- **Evidence:**
  - Removing a capture **disarms its Gate arm** through `gateDisarm` with index, preview and on-air fixup (`llprismlens.cpp:6045-6057`).
  - It also detaches, but keeps, Gate-subscribed displays (`:6031-6037`).
  - The record omits the arm (id, label, enabled, index, on-air state) and the detached Gate displays.
- **Why it breaks:** undo of Remove brings the camera back but leaves it unarmed, with its Gate screens orphaned. The "Gate runtime untouched" claim is false for Remove, and the step is not a true inverse.
- **Fix:** `RemovedCaptureRecord` adds the `GateArmedCamera`, its arm index, the on-air flag and the subscribed-display reattachments. Restore uses a new `gateRearm(record)` that reinstates the index without punching on-air unless it was on-air. Add tests.

### M-R4. B5 residual: the preview quality profile has no mechanism, and the preview scratch pack is not protected from LRU eviction.
- **Doc:** VCAM §4.5 (quality profile; "adds at most one pool entry and cannot churn/evict visible packs"; PASS requires "zero scratch reacquisitions after warm-up").
- **Evidence:**
  - SSAO, shadow detail and volumetrics are global pipeline switches read through `LLCachedControl` statics and shader permutations. For example, `needs_deferred_light = RenderDeferredSSAO || RenderShadowDetail > 0` (`pipeline.cpp:1752`).
  - "Disable for preview-only, keep the visible path byte-identical" requires a new per-render flag consulted at every such site. The doc names none.
  - The pool is 8 entries with LRU eviction (`pipeline.cpp:1739-1793`). Up to 7 visible captures plus adaptive-scale size changes can evict the 192×108 pack, which fails the PASS gate at random.
- **Fix:**
  - Add `LLPipeline::sPrismPreviewRender` and enumerate each gated site (sun/spot shadow generation, SSAO, volumetrics, probe update, far clip), each with a byte-identical off-path proof.
  - Allocate the preview pack as a **dedicated pinned target outside the LRU pool**.

### M-R5. The HDR-to-RGBA8 "same-frame exposure" contract is impossible as written, and it duplicates the `colorCorrect` permutation logic.
- **Doc:** VCAM §4.3 ("same-frame main-view exposure map … no independently sampled next-frame exposure").
- **Evidence:**
  - `renderAuxiliaryView` runs **before** the main view (`llviewerdisplay.cpp:937-939`), so only the previous frame's `mExposureMap` exists at that point.
  - `colorCorrect` selects among at least 8 shader permutations: legacy gamma, no-post, colour grade, tonemap, and the type-8 log curve (`pipeline.cpp:12833-12888`). The bloom pyramid is folded into the tonemap variants.
- **Fix:** define it as the previous presented frame's exposure map plus the current tonemap and grade parameters. Factor permutation selection into one shared helper used by both paths; no second copy. Explicitly exclude bloom, or state it.

### M-R6. Writable-home uniqueness contradicts the data and the P0 spec.
- **Doc:** ROLLOUT §1 ("one home for each control"), §5 (loader "rejects … duplicate writable home"); P0 §3.8 Duplicates ("collapse into the best one, carrying 'also on <pages>'").
- **Evidence:** today's data has 20 settings written from more than one page, for example:
  - `CineRigRimTronMode`/`Mix`/`ColorSource` on Rim and Tron;
  - `BDMergeProjectorVolumetricsFeather`/`MaxDistance` on Rig and Atmosphere;
  - `RenderExposure` and `AlchemyRenderTonemapType` on Exposure and Image;
  - five `CinematicCam*` settings on Moves and Shot, and three across Frame & Orbit/Shot/Operator.
  
  P0 tolerates duplicates; the rollout rejects them; the spec names no home for any of the 20.
- **Fix:** add a `homes` table to `kit_pages.json` (setting to page) for every duplicated setting; the other occurrences generate `link` rows. Make P0 §3.8's "also on" rule apply only to link or read-only occurrences.

### M-R7. The exhaustive audit is an unbounded Cartesian product.
- **Doc:** ROLLOUT §6 ("deterministic Cartesian set pruned only for … mutually exclusive" states).
- **Evidence:** Rig alone has 2 modes × 5 chips × 4 options × 2^(advanced cards) × More states × master × popovers × 4 sizes. Every state costs a layout, an idle, an audit and a screenshot, so the run time is hours to days. Nothing bounds it.
- **Fix:** card layout is width-only and independent. Audit each card's own state set in isolation at each card width the 4 sizes produce, and audit page-level combinations (mode × chip × master × popover) with cards in their tallest state. Publish the state count per page with a hard cap and a run-time budget.

### M-R8. The VCam scene-restore permission policy makes existing saved scenes unloadable.
- **Doc:** VCAM §5.2 ("A live face that fails … `permModify()` rejects the entire scene apply atomically").
- **Evidence:** bindings to non-modifiable faces are legal today (R1 risk 8; `llprismlens.h:580-581`, viewer-local).
- **Why it breaks:** any scene saved before this change that binds such a face now fails completely. That loses every capture in it, and possibly the whole Director scene load, depending on the unspecified coupling.
- **Fix:** load it, then disable the failing binding with a visible reason. Keep reject-atomic only for structural corruption. State how a Prism apply failure affects the Director scene apply (`llfloaterdirector.cpp:1520-1664`).

### M-R9. The bulk-writer barrier has no reachable mechanism from outside the Director.
- **Doc:** P0 §3.11 ("External Lightbox presets and cue playback call the barrier"), §3.7 Lifetime ("no static shell pointer").
- **Evidence:** `ALTron::applyPreset` (called from `alfloaterlightbox.cpp:832`), `ALRotoInk::applyPreset` and cue playback live outside the Director tree, so they cannot "resolve the containing shell from their live view".
- **Why it breaks:** cue playback writes continuously. A per-write barrier would erase undo for the whole time cues play.
- **Fix:** add a static `ALKitHistory::globalBarrier(reason)` that resolves `LLFloaterReg::findTypedInstance<LLFloaterDirector>("director")` at call time. Barrier on cue **start/stop** and on preset apply only, and suspend recording for the duration of playback.

### M-R10. The rollout leaves the largest code-driven pages to one-line descriptions.
- **Doc:** ROLLOUT §2 ("model-facing operations move into a named controller"), §4.
- **Evidence:** C++ that must be split into controllers without a method-level plan:
  | Page | Panel code | Lines | Name lookups |
  |---|---|---|---|
  | Ghosts | `alpanelghoststudio.cpp` | 4167 | 137 |
  | Path | `alpanelpatheditor.cpp` | 2190 | 66 |
  | Dance | `alpaneldanceball.cpp` | 2128 | 78 |
  | Gaze | `alpanellensgaze.cpp` | 2134 | 108 |
  | Shot | `alpanelcinecamparams.cpp` + `llcinematiccamera.cpp` | 826 + 3045 | 259 name references in `llcinematiccamera.cpp` |
  | Diopter | `alfloaterultimatediopter.cpp` | 1589 | — |
  | Actors | `alpanelavatarlightresponse.cpp` | 1023 | — |

  These panels are also used by v1 and by standalone floaters, so the refactor changes v1 C++. That contradicts "v1 behaviour unchanged" unless every extraction is review-diffed. Each matrix row is still one sentence ("all 58 code rows bind through the adapter").
- **Under-specified pages:** Path, Ghosts, Dance, Animate, Switchboard, Gaze, Gaze Cues, Shot, Takes, Diopter, Exposure, Practicals, Actors, Light Cues, Image, Roto, Tron, Actor FX, Atmosphere, Capture, Frame & Orbit, Operator, Move.
- **Adequate today:** Polish, Moves, Focus, Filters, Scene options, Temporal, Weather. Weather's C++ references are mostly setting-name matches in `pipeline.cpp`. Rig and Rim are covered by P0.
- **Fix:** per page, add a controller-extraction table: method moved, from `file:function`, to which controller, the binding for each code row (widget → get/set/default/changed), the commands, and the v1 call sites rewired. Make "v1 behaviour diff per extraction" an explicit review item in each WP.

### M-R11. The WP numbering collides across the three documents.
- **Doc:** VCAM §8 (WP1–WP7, including "WP6 — Director kit page"); ROLLOUT §7 (WP6–WP12, where WP9 includes VCam); P0 (WP0–WP5).
- **Why it breaks:** "WP6" means two different packages, and review hand-offs and acceptance references become ambiguous for Codex.
- **Fix:** rename the VCam packages VC1–VC7 and map them into rollout WP9 explicitly, with ordering. VC1–VC5 are engine work and can precede WP6.

### M-R12. The R1 "superseded" items replaced verified facts with unverified ones in two places.
- **Doc:** P0 §3.14 Frames cites "`alcinelightrig.cpp:2491-2499` object targets skip the avatar-facing assignment".
- **Evidence:** the facing skip is at `:2632-2647`; lines 2491-2499 are the object-target resolve/teardown block. This is minor drift.
- **More important:** the doc now asserts that live offsets "work for object targets". The mirror then uses the facing value that `transformsFromLLSD` / `readSettings` supply for object targets, which `tickShared` never updates. A mirrored, object-targeted rig therefore mirrors about a stale or zero facing in the **engine**, and the map faithfully shows that wrong result.
- **Fix:** fix the anchor. Add an explicit statement (and an in-world check item) for Mirror with an object target: either the engine defines the facing for object targets, or Mirror is disabled for object targets.

---

## MINOR

1. **Transport width proof.** In the 1160 row, 15 visible children give 14 gaps: 112 + 20 = 132, not 124. The status remainder is 320 px, not 328. Correct the table.
2. **Collapse of the REC control.** `rec_indicator_text` is a new name that v1 does not have, and `rec_indicator` is a v1 widget. Say how the 50 → 22 px collapse is implemented without renaming a C++-referenced widget, given the parity class check.
3. **VCam page placement.** Moving VCam from the v1 Lens group to Camera in v2 is fine for `selectLeafTab` (it is name-based, `llfloaterdirector.cpp:809-821`). However, `migrate_tab_name`/`DirectorLastTabName` restore and the v1 to v2 rect/tab carry-over need a test.
4. **ALKitSwitch rebinding.** `setControlName` "connects once". If S9 shows rebinding (`setControlName` called again), the old connection must be replaced, not leaked. Say so.
5. **Arc dial at its minimum size.** At 48×48 the horizon arc occupies x ∈ [6, 26] and leaves the right half empty. The `<w>_value` text sits in the value column, so this is acceptable, but put the mockup comparison item in the §6.7 screenshot review.
6. **Cue-floater name collisions.** The rollout says Light Cues and Gaze Cues are "expanded from the cue stack" while their v1 floaters remain. Two live editors of the same cue model require the owner-token rule from rollout §2. List both floaters as shared owners.

---

## R1 status

| R1 | Status | Note |
|---|---|---|
| B1 all-at-once unspecified | **PARTIAL** | The rollout exists with 36 rows, gates and a ship gate, but the sources and owners are wrong and the custom kinds are invented (N2, N3, M-R10) |
| B2 ALKitSwitch hook | **PARTIAL** | Inner-button signal, `onCommit`, widget defaults and `writeBoundValue` are correct and verified (`llcheckboxctrl.cpp:280`, `llbutton.cpp:1050`). Programmatic `setValue` is still not observed (M-R1) |
| B3 passive mode dead controls | **PARTIAL** | The `applyEasyMode` single path is right. Re-entrancy, instance targeting and the setValue-driven per-instance mode remain (M-R1, M-R2) |
| B4 destructive VCam undo | **PARTIAL** | Typed identity-preserving undo is correct, and `applySceneData` is banned. Remove/undo loses the Gate arm (M-R3) |
| B5 GPU-only budget | **PARTIAL** | CPU+GPU charge, p95 delta, fixed bucket and a three-phase self-test are good. The quality-profile mechanism and pack pinning are missing (M-R4) |
| B6 incompatible VCam page / two histories | RESOLVED | Kit page, frozen v1, sole `ALKitHistory`; VCam §5.4/§6.1 verified |
| M1 cadence stealing | RESOLVED | Two-tier fill; preview deadline in Every Frame mode (`llprismlens.cpp:3096-3122`, `3185-3197` cited correctly) |
| M2 SOURCE_VIEWER stale toolbar item | RESOLVED | `SOURCE_VCAM`; the `drop=true` fix is noted |
| M3 Ctrl+Z bubbling and `/` | RESOLVED | Text-focus guard; `/` handled in `handleUnicodeCharHere` |
| M4 bulk-writer history | **PARTIAL** | The barrier is specified, but there is no route from external writers and no per-write policy for cue playback (M-R9) |
| M5 settings graph | RESOLVED | The `Include` mechanism is real (`llcontrol.cpp:1034`; `settings.xml` Include of `settings_alchemy.xml`); `findControl` (`llview.cpp:2405`) |
| M6 mirror rig-off | **SUPERSEDED** | The rig-off fallback is now consistent, but the live-offset source introduces N1 and M-R12 |
| M7 Side-drag inverse | **SUPERSEDED** | The fallback maths is fine; the live inverse uses the wrong radius and is relative (N1) |
| M8 arc geometry | RESOLVED | Horizon geometry fits 48×48: r = 20, centre (6, 24), maximum label within the bounds |
| M9 audit coverage | **SUPERSEDED** | Coverage is now exhaustive but unbounded (M-R7); label-metric errors are resolved |
| M10 transport at 850 | RESOLVED | The 850 arithmetic checks out (576 + 116 + 158); minor error in the 1160 row (MINOR 1) |
| M11 WP1/WP2 interfaces | RESOLVED | Headers and stubs in WP1 |
| M12 lifetime | RESOLVED | Handles, shell-owned timers, no static shell (external barrier: see M-R9) |
| M13 parity class/dynamic names | RESOLVED | Patterns added for `alpanelcinelightrig.cpp:1064-1074` plus a runtime log; the source set is still wrong (N2) |
| M14 GL state / HDR transform | **PARTIAL** | The state-guard contract is good; the exposure timing is impossible (M-R5) |
| M15 menu exposure | RESOLVED | No menu item until WP12 |
| M16 index fields | RESOLVED | Version 2, `fields="15"`, shared constant |
| MIN1 `min` → `minimum` | RESOLVED | `aluiperfselftest.cpp:1320-1323` verified |
| MIN2 dial legacy geometry | RESOLVED | `kit_geometry` defaults to false |
| MIN3 0..360 wrap | RESOLVED | |
| MIN4 legend text in draw | RESOLVED | 10 Hz clock with diff guards |
| MIN5 64-row probe / setVisible | RESOLVED | |
| MIN6 reshape lookup | RESOLVED | `reshape` at `:2811` verified |
| MIN7 scrollbar reserve | RESOLVED | |
| MIN8 fixture role commit | RESOLVED | |
| MIN9 visibility owner tokens | RESOLVED | |
| MIN10 prepare decides | RESOLVED | |
| MIN11 scene restore permission | **UNRESOLVED (over-corrected)** | Now regresses existing scenes (M-R8) |
| MIN12 output release wording | RESOLVED | |
| MIN13 `GL_TIMESTAMP` | RESOLVED | `llglslshader.cpp:263-288` |
| MIN14 cargo static_assert | RESOLVED | |

Tally (36 R1 items): 25 RESOLVED, 7 PARTIAL (B1-B5, M4, M14), 3 SUPERSEDED (M6, M7, M9), 1 UNRESOLVED (MIN11). B1's implementability gap is carried forward as N2 and N3.

## New claims verified OK in R2

- `widgets/check_box.xml:11-24` (button rect and `is_toggle`); `llcheckboxctrl.h:144-147` (`mButton` is protected and accessible).
- `settings.xml:6327-6330` `Include` → `settings_alchemy.xml`; `LLControlGroup` handles `Include` (`llcontrol.cpp:1034`).
- `LLView::findControl` at `llview.cpp:2405-2427`.
- `auditSizeName()` at `aluiperfselftest.cpp:1320-1323` returns "minimum".
- `alpanelcinelightrig.cpp:2811` `reshape`; `:2853` per-frame `syncEasyModeForSelected()`; `:1064-1074` dynamic role combos.
- `lltooldraganddrop.cpp:573-595` people drop pass with `drop=false`; `lltoolbarview.cpp:949-966` the cached `mDragItem`.
- `pipeline.cpp:12833` `colorCorrect` with its permutation set; `llviewerdisplay.cpp:939` aux render before the main view.
- `llprismlens.cpp:5503-5579` (destructive `applySceneData`) and `:6031-6057` (removal disarms the Gate).
- All 22 panel classes the rollout names exist (`LLPanelInjector` registrations). Switcher slot count 12 (`aldirectorswitchermodel.h:20`).
- The page count is 5+5+6+3+6+5+3+3 = 36 and matches the data groups (VCam is in `camera_group` in the data).
