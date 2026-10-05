# Director console audit (performance + structural traps)

Read-only audit by Fable, 2026-10-05. Brief: `doc/DIRECTOR_CONSOLE_AUDIT_BRIEF.md`.
Written incrementally; sections are appended as evidence is gathered. No code was edited, built or committed.

Code baseline: working tree at HEAD (after d591170ed68 "UI panels: diff-guarded draw() writes" and bd6ef320f69 / 30af60abc28 Ghost Studio fixes).
Tracy baseline: `C:\Users\xianw\Desktop\Tracy\captures\alchemy_20261005_103942.tracy` (10:39, BEFORE the fixes, Director open) and the per-call exports in `C:\Users\xianw\AppData\Local\Temp\ghostaudit\`. Re-exports for this audit go to `C:\Users\xianw\AppData\Local\Temp\directoraudit\`.

## 0. Method and limits

- Evidence is file:line from the current tree, git history, and the Tracy capture. Nothing was tested in-world.
- Cost estimates are per frame at a 60-120 Hz UI frame rate unless stated, and are labelled with a confidence level: **high** (measured in Tracy or counted directly), **medium** (counted code paths with known widget costs), **low** (inference).
- "Hot path" means anything reachable from `LLFloaterDirector::draw()` or a child panel's `draw()` on every frame while the floater is visible.
- Line numbers are from the working tree at HEAD 1c560897e17 (branch fix/animesh-clone-pose-polish). All four commits named in the brief (9f31a1f2058, 30af60abc28, bd6ef320f69, d591170ed68) are ancestors of HEAD.

## 1. The reconfigure, from git history

`git log -- llfloaterdirector.cpp floater_director.xml` shows the console was rebuilt in a series between 2026-08-01 and 2026-08-30; the last structural step is the Virtual Cam consolidation. Everything since is additive (menu items, scene keys) plus the Part C diff. What each step introduced, and the assumption it carries:

| Commit | Date | Change | Assumption introduced |
|---|---|---|---|
| 756fedf7a90 | 08-01 | "camera-tab reflow": tab heights 360 -> 550; Camera tab went from one scroll document (`camera_scroll_content` h=1397) to `camera_flow_scroll` + `<flow_grid>` with cards; `move_scroll`/`animate_scroll` documents appeared; Subject C/D rows | cards and scroll documents get hand-typed heights (card = embedded panel + 36) |
| 7aabfa69093 | 08-04 | Prism camera feed; `ALScrollFocus::install` added to the Director for Move/Animate (later Actor FX, Shafts) | focus-reveal only on the four containers named in postBuild |
| 50816fa5c54 | 08-19 | Lens Gaze moved out to the Actor Gaze floater (-216 XML lines); `refreshCameraTab` now pushes the cast selection to that floater every frame | cross-floater per-frame push via `LLFloaterReg::findTypedInstance` |
| f547400e0fb | 08-24 | Actor FX tab (`actor_style_scroll`, content 570, now 596) and the Shafts tab scroll document (1360, now 1392); Pose Polish button removed | the Actor FX refresher was written after the other refreshers and was not in Part C's list |
| 3db1c1b4a94 | 08-29 | "Consolidate Virtual Cam controls into Director": `floater_virtual_cam` retired; Shot Builders card (226), VCam enable + summary row, `camera_flow_scroll` 371 -> 343; +214 lines in the cpp (OTS pair, Zolly, Hero Arc, Prism summary) | Camera tab has a fixed 205 px header above the card scroller |
| 35cd2ef0d13 .. 9ebee5b54c5 | 08-29/30 | Path V2: `path_scroll_content` 736 / `path_editor` 720 | more pinned heights |
| d591170ed68 | 10-05 | Part C: three `setTextIfChanged` swaps (animate header, 4 subject texts, status strip) + a Tracy zone on `draw()` | nothing else in the Director was changed |

Resulting shape (floater_director.xml): 12 left-rail tabs in `director_tabs` (h=550, l.313-324), 16 distinct shared panel files embedded as 18 instances (two `panel_cinecam_params` class instances with different XML files, l.1285-1294 and 1381-1390), 7 Camera cards, 8 scroll containers of which 7 have hand-typed document heights, 2 tabs with no scroll container at all (Ghosts l.557-576, Props l.583-602) and 2 fixed tabs (Takes l.1401-1420, Time l.1785-1804). Every embedded `<panel filename=...>` node pins `height=` on the shared panel (the host attribute overrides the file's root: e.g. `panel_cinecam_params.xml` is 303 tall by its own root, 420 as embedded at l.1289).

## 2. Performance

### 2.1 What the Tracy capture says (10:39, before the fixes, Director open)

Source: `alchemy_20261005_103942.csv` (aggregate) and the per-event exports in `ghostaudit\` (303 `LLAppViewer::doFrame` events over 20.0 s; mean 66.1 ms, min 15.6 ms, max 399 ms). Per-floater `LLFloater::draw` totals (floater title is the zone text):

| Floater | calls | total ms | mean us | max us | ms/frame |
|---|---|---|---|---|---|
| Director Console | 305 | 10,303.7 | 33,782 | 357,933 | 34.0 |
| CONVERSATIONS | 305 | 114.2 | 374 | 999 | 0.38 |
| Nearby chat | 305 | 87.3 | 286 | 919 | 0.29 |
| Animation Overrider | 305 | 30.2 | 99 | 272 | 0.10 |
| all others | 305 each | < 8 each | < 26 | < 132 | < 0.03 |

Director Console `LLFloater::draw` distribution: 211 frames in 0.25-0.5 ms (83.5 ms total), 53 frames in 0.5-1 ms (35.2 ms), 2 frames in 1-2 ms, 10 frames in 2-5 ms (42.4 ms), 0 frames in 5-100 ms, **29 frames in 100-1000 ms (10,140 ms)**. Percentiles: p50 412 us, p90 4.3 ms, p99 356 ms.

- The 29 hitch frames are the Ghost Studio rig-diagnostics `setText` (`LLTextBase::appendText` aggregate: 4,832 calls, 9,972 ms; the per-event export shows 29 calls above 50 ms). The Ghosts tab was the visible Director tab during the capture. Both halves of that are fixed in HEAD: `appendText` is linear (30af60abc28) and `refreshRigDiagnostics` is revision/on-screen/2 Hz gated with a 32768 cap (`alpanelghoststudio.cpp:1823-1893`).
- Without the hitch frames the Director's `LLFloater::draw` median is ~0.41 ms and ~0.44 ms mean. That number includes drawing every visible widget of the open tab; CONVERSATIONS (a text-heavy floater) costs 0.37 ms for comparison. There are no per-refresher zones, so the split between "refreshers" and "drawing" is an estimate: **~0.1-0.25 ms/frame of refresher CPU** at a 6-member cast (section 2.2 sums to that), the rest is widget drawing.
- The 10 frames at 2-5 ms cannot be attributed from this capture (no zones inside the Director). Candidates: list rebuilds on a selection/animation change (`deleteAllItems` + `addElement` per row), the Ghost panel's pre-fix `refreshDetail`, or the switcher re-parse on the Camera tab. Needs a post-fix capture (checklist item 10).
- UI-wide numbers that include the Director's children: `LLView::findChildView` 163,433 calls (536/frame, 0.41 ms/frame), `LLScrollContainer::draw` 1,830 calls (6/frame, 53 us each, 0.32 ms/frame), `LLTextBase::draw` 21,045 calls (69/frame, 0.46 ms/frame), `LLTextBase::reflow` 63,135 calls (208/frame, 0.03 ms/frame), `LLUIString::updateResult` 6,487 (21/frame). Part C cached the Lightbox and Cine Light Rig by-name lookups after this capture, so `findChildView` should be well below 536/frame now; this audit found no by-name lookup left in any Director draw path (section 2.3), but it is unverified until the next capture.
- Whole-UI `UI 2D` minus the appendText hitches is ~0.8 s / 303 frames = **~2.7 ms/frame for every floater together**; the Director's non-hitch share is ~0.44 ms of that.

Bottom line: after d591170ed68 and bd6ef320f69 the Director is not a hitch source. What is left is rule compliance (asString, unguarded setText, by-name settings reads) and ~40-60 heap-allocating `std::string` temporaries per frame, worth ~0.1-0.25 ms, i.e. about 1 % of a 60 Hz frame.

### 2.2 The nine refreshers in `LLFloaterDirector::draw` (llfloaterdirector.cpp:652-666), every frame, every tab, visible or not

Costs are per frame at a 6-member cast with ~10 signaled animations; confidence in brackets.

| # | Refresher | What it does per frame | Est. | Rule violations / notes |
|---|---|---|---|---|
| 1 | `refreshCastList` 1440-1599 | `getAllData()` copies the item vector (1448); per row: `cast.resolve` (findObject, 1505), `isMoving` (map, 1507), `castMemberName` 1418-1438 (getMember, then `LLGhostAvatar::isGhostId` = another `findObject` 1425, `LLAvatarNameCache::get` copies an LLAvatarName + `getCompleteName` builds a string 1429-1431), name/ab/mark/look strings built then diffed against `CastRowState` (good: cells only re-set on change); 5 `getColumn(name)` map lookups (1492-1496); `firstSelectedCastId` twice; `castMemberName` again for the Focus tooltip (1597-1598) | 25-35 us [medium] | **`id.asString()` in draw** for every ghost / animesh / unresolved row (1427, 1436-1437), every frame |
| 2 | `refreshGroupControls` 1917-2040 | `cast.getGroupNames()` allocates a vector (O(cast x groups), lldirectorcast.cpp:1762); `names != mLastGroupNames`; `membersInGroup(run_sel)` allocates (1962); snapshot = per group `llformat` + `membersInGroup(name)` (alloc) + `getGroupDelay` (1985-1992); ~8 tooltips built as `std::string` temporaries before compare | 10-25 us [medium] | string-snapshot guard (rule 2), but Part C dropped cast/group revisions on purpose; `mLastGroupNames.assign(1, "")` sentinel relies on "" never being a group name (1811, 1893, 1911) |
| 3 | `refreshTransport` 719-799 | 4 `LLCachedControl` reads; `fires` concatenation; ~8 tooltip strings; `getSelectedItemLabel()` copy; cast loop for marks; `mCutBtn->setLabel` every frame while counting (736) | 5-10 us [medium] | fine |
| 4 | `refreshMoveTab` 2045-2058 / `refreshPathTab` 2063-2072 | `selectedCastIds()` walks the list and allocates; `setSelectedActors` copies into the panel; `setTargetActor` diffs internally (alpanelpatheditor.cpp:267-277) | 1-3 us [high] | pushed into hidden tabs too (harmless, cheap) |
| 5 | `refreshActorStyleTab` 2152-2320 | **roster signature string every frame**: `self_id.asString()` + per member `mId.asString()` + name + `cast.resolve` (2170-2182); 15-field `llformat` snapshot + compare (2250-2260); ~17 `setEnabled`; `castMemberName(id)` + `cast.resolve(id)` again (2308-2310); **`mActorStyleStatus->setText(...)` unguarded every frame** (2317-2319, and 2241 in the no-target branch) | 15-30 us [high] | rule 1 x2 (asString in draw, setText in draw). This tab was added 08-24 and was never in Part C's list |
| 6 | `refreshAnimateTab` 2618-2794 | `castMemberName(sel)`; `snap.assign(mSignaledAnimations)` + vector compare (2633-2640); `getAllData()` copy (2673); per row `findMotion` + `llformat("%d")` + `mPlayingAnimations.find` (2682-2688), cells diffed; **`mAnimHint->setText(hint)` unguarded every frame whenever a hint is showing** (2723-2727: no selection / away / no anims, i.e. the idle state); `pasteAnimId()` = getText copy + trim + `LLUUID::validate` (2754-2756); ~9 tooltip strings; `refreshLocomotionRoleEditor` 2519-2616 builds status strings, `cast.resolve` x2, `findMotion`, phase info (writes are guarded); `animSourceObject` walks `mAnimationSources` (2816) | 20-40 us [medium] | rule 1 (setText in draw) on a word-wrapped text box = one appendText + reflow per frame in the common idle state |
| 7 | `refreshCameraTab` 2931-3104 | `ALFloaterActorGaze::updateSelection(DIRECTOR, selectedCastIds())` = alloc + `LLFloaterReg::findTypedInstance("actor_gaze")` by name + vector copy into the gaze panel when open (alfloateractorgaze.cpp:39-58, alpanellensgaze.h:42); 4 x `castMemberName` + 4 concatenations into `setTextIfChanged`; `avatarFromSelection()`; 8 tooltips; early-out when hidden (2987); visible: `can_start_manual_subject_shot` + `resolveSubjectA/B` (findObject x3), Prism snapshots at 4 Hz (3016-3025) | 8-15 us hidden, 15-25 us visible [medium] | by-name floater lookup in a draw path, every frame; otherwise the best-behaved refresher (visibility early-out + revision-gated Prism summary) |
| 8 | `refreshStatusStrip` 3109-3191 | `isMoving` per id; `castMemberName`; 2-3 `llformat`; diffed write; REC indicator diffed | 3-6 us [high] | fine |
| 9 | `LLFloater::draw` | draws the visible tab's widgets | the remaining ~0.25-0.3 ms | - |

Sum of 1-8: ~90-170 us/frame plus ~40-60 small heap allocations, independent of which tab is visible. Only #7 checks visibility. No refresher uses `getChild`/`findChild` by name (all widget pointers are cached in postBuild, 278-606) and no `gSavedSettings.getXxx("name")` is read per frame in the Director proper (`LLCachedControl` everywhere: 721-724, 760, 2433, 3111-3113, 3136).

### 2.3 Child panels (draw only while their tab is visible; `LLTabContainer` hides the rest: lltabcontainer.cpp:1036, 1583)

| Tab | Panel / draw | Per-frame work while visible | Est. | Status vs Part C |
|---|---|---|---|---|
| Camera | `ALPanelDirectorSwitcher::draw` alpaneldirectorswitcher.cpp:206-213 | `refreshBank(false)` 462-473 -> `loadNormalizedBank` 216-233 -> `ALDirectorSwitcher::loadBank` (aldirectorswitcher.cpp:168-240: `gSavedSettings.getLLSD("DirectorSwitcherBank")` by name = settings-map lookup + deep copy of 12 maps x 11 keys, `defaultBank()`, sanitize, 12 label strings) then `banksEqual` 236-260; `refreshProgramState` 536-581 reads `gSavedSettings.getBOOL("DirectorSwitcherArmed")` by name (539) | 40-80 us [medium] | **still present**; dropped from Part C v3 on purpose (`resetToDefault(false)` / group reset fire no signal) |
| Camera | `ALPanelDirectorAnimSwitcher::draw` alpaneldirectoranimswitcher.cpp:114-122 | same shape: `loadBank` + 12 slots incl. pose-name strings, `banksEqual` 144-...; `getBOOL("DirectorAnimSwitcherArmed")` by name (535) | 40-80 us [medium] | **still present** (same decision) |
| Camera | `ALPanelCineCamParams` x2, `ALPanelFlycamOrbit`, `panel_cinecam_frame` | no draw overrides; `updateModePanel` (alpanelcinecamparams.cpp:511-524) runs ~60 `findChild` by name only on the CinematicCamMode signal / visibility change | ~0 | fine |
| Animate | `ALPanelAnimPreview::draw` alpanelanimpreview.cpp:83-160 | `mAnimationPreview->requestUpdate()` (138) renders the dummy avatar to a texture every frame; `findMotion` + loop re-arm; `refreshActionButtons` builds 5 tooltip strings | 0.3-1 ms CPU+GPU [low-medium] | by design (stock Animation Explorer idiom); the heaviest per-frame item on any tab. The dummy (created lazily at 86-91) is never released while the floater lives; a second one exists when the Animation Explorer is open |
| Props | `ALPanelPropMover::draw` alpanelpropmover.cpp:129-137 | every 0.5 s `refresh()` 160-215: `clearRows` + rebuild both lists, unguarded `mStatusText->setText`, spinner/combo `setValue` (253-290) | 0.5-1 ms per rebuild at ~10 props, i.e. 1-2 ms/s [medium] | **still present**; dropped from Part C (paths publicly mutable) |
| Path | `ALPanelPathEditor::draw` alpanelpatheditor.cpp:306-336 | 11 refreshers, writes converted by Part C; `LLActorMover::instance().setEditActor(mActor)` every frame (313) | small; not re-measured | converted; see trap T4 |
| Ghosts | `ALPanelGhostStudio::draw` alpanelghoststudio.cpp:917-1018 | revision gates + clocks + diff-guarded writes; `isWidgetOnScreen` for the diagnostics | small | **fixed** (bd6ef320f69) |
| Lights | `ALPanelCineLightRig::draw` alpanelcinelightrig.cpp:2760-2777 | `updateAnchorList`, `syncGroupControls`, `syncSeedEditor`, `syncFixtureControls`, `syncGoboLibrary`, `updateDerivedStatus` | not re-audited here | converted by Part C (cached control arrays, widget-aware compares) |
| Weather | `ALPanelWeatherSettings::draw` alpanelweathersettings.cpp:151-155 | `updateDeferredAvailability` 190-224: pointers now cached, but `gSavedSettings.getBOOL("AlchemyWeatherEnabled")` and `getBOOL("AlchemyWeatherLightningEnabled")` by name every frame (221-222) | ~1-2 us | rule 1 (settings by name in draw) |
| Volumes | `ALPanelAtmosphericVolumes::draw` alpanelatmosphericvolumes.cpp:101-113 | revision-gated `refreshBank` + selection compare | ~0 | good model |
| Time | `ALPanelTemporalCapture::draw` alpaneltemporalcapture.cpp:75-121 | 3 `llformat` strings, each compared to a cached copy before writing | ~2 us | fine |
| Takes | `ALPanelFlycamRecorder::draw` alpanelflycamrecorder.cpp:82-137 | all writes diff-guarded, max-before-value | ~3 us | **fixed** (Part C) |
| Shafts, Actor FX, Move | XML-bound sliders / the Director's own refreshers / `ALPanelActorMover::draw` 53-87 (2 cached controls, 2 tooltip strings) | ~2 us | fine |

### 2.4 Idle callbacks, signals, timers registered by the Director or its children

- Director: none beyond `LLFrameTimer mPrismRefreshTimer` (4 Hz Prism snapshot). No `gIdleCallbacks`, no `LLEventTimer`.
- `ALPanelDirectorSwitcher`: two `boost::signals2::scoped_connection` on setting commit signals (alpaneldirectorswitcher.cpp:94-111; header 94-95), fire only on change, auto-disconnect. `ALPanelCineCamParams`: `mModeConnection` on `CinematicCamMode`, disconnected in the dtor (382-388); three live instances when the Director and the standalone Cinematic Camera are both open, each running `updateModePanel` (60 by-name `findChild`) per mode change. `ALPanelCineLightRig`: `mNightMaskDarknessConnection` scoped (header 145). All lifetimes are correct.
- Not Director-hosted but fed by it: `ALFloaterActorGaze::updateSelection` (every frame from `refreshCameraTab`); the Lens Gaze panel also owns a permanent `gIdleCallbacks` function (alpanellensgaze.cpp:356-358) that runs with its floater closed (out of scope here, noted for completeness).

### 2.5 Hidden tabs and double hosting

- Hidden tabs: child panels do not draw (LLTabContainer sets non-selected panels invisible; `LLView::drawChildren` skips invisible children), so only the Director's own nine refreshers run for hidden tabs. All eight non-Camera refreshers run regardless of which tab is showing; the Animate list (`findMotion` per row) and the Actor FX roster string are the two that do real work for a hidden tab.
- Double hosting: every embedded panel except the two switchers and `panel_cinecam_frame` also has a standalone host (floater_actor_mover.xml hosts both `panel_actor_mover` and `panel_path_editor`; floater_ghost_studio, floater_prop_mover, floater_animation_explorer, floater_cinematic_camera x2, floater_flycam_orbit, floater_flycam_recorder, floater_cine_light_rig, floater_lightbox_settings x2 (weather, volumes), floater_temporal_capture). With a standalone open, that panel's draw work runs twice; the only expensive duplicates are the Prop Mover's 0.5 s rebuild and the Anim Preview's second dummy render (~0.3-1 ms/frame).

## 3. Structural traps (ranked)

Severity: **S1** breaks a visible feature today; **S2** breaks on the next ordinary edit or at a non-default size; **S3** hygiene / rule violation with no user-visible failure yet.

### T1 (S1) Props tab: no scroll container, content taller than the tab -> bottom row spills out of the floater
- floater_director.xml:583-602: `props_tab` h=676 inside `director_tabs` h=550; `prop_mover_panel` pinned to h=660 at top=8 with `follows="left|top|right"`.
- `LLTabContainer::addTabPanel` reshapes every tab panel to its own panel rect and sets `setFollowsAll()` (lltabcontainer.cpp:1031-1033): for left tabs that is the container height minus 2 px = **548 px at the default 650-px floater, 458 px at `min_height` 560**. The 676 is ignored; the embedded panel keeps 660 (it follows top only), so it extends 8+660 = 668 px into a 548-px tab.
- Nothing clips it: `LLTabContainer::draw` clips only its tab-button strip (lltabcontainer.cpp:471) and `LLView::drawChildren` tests only the root rect (llview.cpp:1268). The Prop Mover's Shuttle row (`shuttle_header` top=546, axis/sign/slope combos top=564, distance/speed/`btn_shuttle` top=586-588, `status_text` top=618; panel_prop_mover.xml:488-610) draws **over the Director's status strip (y=632) and past the floater's bottom edge (650)**. Controls below the floater edge are visible but unclickable (events do not reach a view outside its floater), the ones over the strip are clickable but overlap it. At min height the whole spin/shuttle section (top >= 450) is affected.
- Standalone host for comparison: floater_prop_mover.xml is 692 tall (min 452) and shows all 660 px.
- Failure scenario: user opens Props in the Director, sees the Shuttle controls floating below the frame or cannot reach "Shuttle"/"Dist m"; shrinks the floater and loses more. Exactly the Lightbox class. Confidence: high (geometry from XML + stock tab-container code; not seen in-world).

### T2 (S2) Seven hand-typed scroll documents and 18 host-pinned panel heights
- Scroll documents with typed heights: `move_scroll_content` 540 (content ends at 539), `animate_scroll_content` 560 (544), `actor_style_scroll_content` 596 (586), `projector_volumetrics_scroll_content` 1392 (1384), `path_scroll_content` 736 (728), `weather_scroll_content` 710 (710), `atmospheric_volumes_scroll_content` 660 (650) (floater_director.xml:346-354, 630-638, 1828-1836, 1444-1452, 529-536, 1715-1722, 1756-1763). Margins are 0-10 px: the next control added to any of them clips or sits under the scrollbar's reserved corner. The Lightbox had exactly this (9f31a1f2058).
- Every `<panel class=... filename=...>` node pins `height=` on the shared panel, and the host attribute wins over the file's root (proof: `panel_cinecam_params.xml` root h=303, embedded h=420 at l.1289; `panel_ghost_studio.xml` root 760, embedded 534 at l.570). So a shared panel that grows in its own XML (the normal way these panels evolve: weather, volumes, path, temporal, recorder all grew in August) is silently cut in the Director while its standalone host shows it whole. Current pins: actor_mover 360/360, path_editor 720/720, ghost_studio 534/**760**, prop_mover 660/660, anim_preview 188/188, director_switcher 640/640, anim_switcher 336/336, cinecam_params 420/**303**, flycam_orbit 214/214, cinecam_frame 758/758, cinecam_operator 1325/1325, flycam_recorder 314/314, weather 710/710, atmospheric_volumes 650/650, temporal 360/360.
- Camera cards: card height is hand-typed as embedded + 36 in seven places (676/640, 226, 372/336, 456/420, 250/214, 794/758, 1361/1325; l.1123-1391). `ALFlowGrid::reshape` packs columns by each card's declared height (alflowgrid.cpp:102, 124, 190-197) and never reads a card's children, so a card whose embedded panel grows **overlaps the next card in its column** instead of pushing it down. The Cinematic card already shows drift the other way: 117 px of blank space (420 pinned vs 303 real; the operator tuning moved to its own file and left a `panel_operator_tuning` h=0 stub at panel_cinecam_params.xml:1464).
- Failure scenario: Codex adds one slider to panel_weather_settings.xml (Lightbox tab looks fine because 9f31a1f2058 auto-fits it); the Director's Weather tab clips the new slider; or a Shot Builders control is added and the Switcher card draws over it. Confidence: high.

### T3 (S2) Ghosts tab viewport: pinned 534 px vs 760 own
- floater_director.xml:566-575 pins `ghost_studio_panel` to 534 (follows all). Inside, `instances_section` (214, top-anchored) keeps its height and `ghost_studio_tabs` (follows all, 520 at top=214; panel_ghost_studio.xml:50-59) shrinks to ~294, so each Ghost Studio sub-tab scroll viewport is ~264 px (176 px at the Director's min height) for 282-790 px of content (`pose_section` 790, l.204). It does not clip (the inner `scroll_container`s follow all), but every Ghost Studio workflow inside the Director scrolls constantly, and the standalone host (floater_ghost_studio.xml 794 tall, 490-px viewports) behaves differently. This is also why the rig-diagnostics editor sat off-screen in the Director in the prior audit.
- Confidence: high.

### T4 (S1 when both hosts are open) Path editor claims singleton edit state from draw(), releases it on hide
- alpanelpatheditor.cpp:313 `LLActorMover::instance().setEditActor(mActor)` **every frame** while visible ("this panel owns the shared edit-selection actor"); 481-492 `onVisibilityChange(false)` -> `exitEditMode()`, `LLPathCamera::instance().stopPreview()`, `setEditActor(LLUUID::null)`; the dtor does the same (65-70).
- The same class is hosted by floater_actor_mover.xml (llfloateractormover.cpp:40, 223 feeds `setTargetActor(selectedActor())`).
- Scenarios: (a) Actor Mover floater on its Path tab and Director on its Path tab with different selections: both write the edit actor each frame, so the in-world node highlight and the edit tool's target alternate with floater z-order (last drawn wins). (b) Director on Path, standalone previewing a path camera: user clicks another Director tab -> the now-hidden Director path panel calls the global `stopPreview()` and nulls the edit actor -> the standalone's camera preview dies and its highlight blinks until its next draw re-claims. The Ghost Studio solved this class with a per-panel `mPlacementOwner` (alpanelghoststudio.cpp:940-942, 690) and owner-gated releases; the path editor has no owner token. Confidence: high (code), untested in-world.

### T5 (S3) `refreshActorStyleTab` is the one Director refresher nobody converted
- llfloaterdirector.cpp:2170-2182 builds a roster signature with `LLUUID::asString()` for self and every member, every frame (rule 1); 2250-2257 `llformat`s a 15-field snapshot every frame to detect change; 2317-2319 and 2241 call `mActorStyleStatus->setText(...)` unguarded every frame (rule 1). The tab was added in f547400e0fb (08-24) after the Part C target list was written; Part C touched lines 2625, 2941-2947, 3169 only.
- Failure: no visible bug; it is the exact pattern that produced the 345 ms hitch elsewhere, sitting one text-box-size change away (the status box is 34 px, two lines). Confidence: high.

### T6 (S3) `refreshAnimateTab` hint `setText` every frame in the idle state
- llfloaterdirector.cpp:2723-2727: `if (!hint.empty()) mAnimHint->setText(hint)` runs every frame whenever no member is selected, the member is away, or has no signaled animations - the common state. A word-wrapped `LLTextBox::setText` is an `appendText` + `reflow` per frame. Part C missed it because the write is inside a conditional. Confidence: high.

### T7 (S3) By-name lookups and settings reads in draw paths of hosted panels
- `ALPanelDirectorSwitcher::refreshProgramState` 539 and `ALPanelDirectorAnimSwitcher::refreshProgramState` 535: `gSavedSettings.getBOOL("...Armed")` every frame. `ALPanelWeatherSettings::updateDeferredAvailability` 221-222: two `getBOOL` by name every frame. `refreshCameraTab` 2933-2935: `LLFloaterReg::findTypedInstance("actor_gaze")` every frame. Director proper: all widget pointers cached; the only draw-path by-name lookups are `mCastList->getColumn("name")` x5 (1492-1496) and `mAnimList->getColumn` x2 (2671-2672), which are the scroll list's own `std::map<string>` (llscrolllistctrl.cpp:3274-3282), not `findChildView`.
- Why it matters more here than elsewhere: `LLTabContainer::getChildView` (lltabcontainer.cpp:317-342) makes any by-name lookup from the Director root walk all 12 tab panels recursively (~1,500 widgets). A future `getChild` in a Director refresher costs 10-50 us each, not 1 us. Confidence: high.

### T8 (S3) Dummy-widget avoidance by 1-px stubs (works only because)
- `ALPanelCineCamParams::postBuild` uses `getChild<LLButton>("btn_reset_mode"/"btn_reset_all")` (alpanelcinecamparams.cpp:396-399) while the Camera Shake card instantiates the same class on `panel_cinecam_operator.xml`; that file carries two invisible `height="1"` buttons with those names (panel_cinecam_operator.xml:389-391) purely so `getChild` does not create a dummy (`LLView::getChild` -> "Making dummy ..." llview.h:716-730). Delete the stubs and the warning (and a hidden dummy with a live commit callback) returns. The rest of the class correctly uses `findChild` for optional widgets (400-424). Confidence: high.

### T9 (S2) Floater minimum size is sized for the chrome, not the tabs
- floater_director.xml:11-13: `min_height="560"`, `min_width="660"`, `save_rect="true"`. `LLFloater::applyRectControl` clamps a restored rect to the minimum (llfloater.cpp:1007), so a stale saved rect cannot go below it, but at the minimum the tab area is 458 px: Props spills 210 px (T1), Ghost viewports are ~176 px (T3), the Camera tab's fixed 205-px header leaves ~250 px for the card scroller. Nothing ties `min_height` to the tallest fixed tab (Takes 314, Time 360 are fine). Lightbox had the same shape. Confidence: high.

### T10 (S3) Focus-reveal installed on 4 of 8 scroll containers, non-idempotently
- llfloaterdirector.cpp:536-551 installs `ALScrollFocus` for Move, Animate, Actor FX, Shafts only. Path (`path_scroll`), Weather, Volumes, Lights, Camera (`camera_flow_scroll`) have none, so Tab-key navigation can leave the focused control off-screen there. `ALPanelCineCamParams` installs for itself via `installAncestor` (alpanelcinecamparams.cpp:394), so two Camera cards are covered and five are not. `ALScrollFocus::install` connects another slot on every call (noted at alfloaterlightbox.cpp:389-390), so a generic "install on every container" pass must de-duplicate as the Lightbox does (focus_done, 391-422). Confidence: high.

### T11 (S3) Tab persistence by positional index
- `onTabChanged` 636-642 saves `getCurrentPanelIndex()`; `onOpen` 617-623 clamps and restores. The XML declares Actor FX last (l.1809) while the icon table lists it fourth (318-327); any reordering of `<panel>` nodes silently changes which tab reopens and the clamp hides a shrink. Confidence: high.

### T12 (S3) Per-frame cross-floater push instead of change-driven
- `refreshCameraTab` pushes `selectedCastIds()` into the Actor Gaze floater every frame (2933-2935 -> alfloateractorgaze.cpp:39-58 -> `mSelected = ids`). Harmless today (source-gated inside), but it is the only Director path that mutates another floater from draw(), and it allocates a vector per frame to do it. Confidence: high.

### T13 (S3) Lifetime review: nothing dangling
- Context menus are parented to `gMenuHolder` and killed in the dtor (254-269); the scene-delete notification captures an `LLHandle` (1086-1103); all setting-signal connections are scoped or disconnected in dtors (section 2.4); `ALFloaterActorGaze::updateSelection` resolves by registry each call. The one ordering smell: `~ALPanelDirectorSwitcher` and `LLFloaterDirector::onClose` both call `ALDirectorSwitcher::instance().cancelEaseWorldTime()` (alpaneldirectorswitcher.cpp:56-59, llfloaterdirector.cpp:271-276), which can re-create the singleton during shutdown teardown. The floater is `single_instance` so closing hides rather than destroys (llfloater.cpp:275: reuse defaults to single_instance), which means the 18 embedded panels, their signal slots and the Anim Preview dummy live for the whole session once the Director has been opened; per-frame cost while hidden is zero (draw is not called). Confidence: high.

### T14 (S3) Group-assign sentinel
- `mLastGroupNames.assign(1, std::string())` as an "impossible value" (1811, 1893, 1911) relies on `getGroupNames()` never returning "" (lldirectorcast.cpp:1766-1775 filters empties, so it holds). Documented in a comment; works only because of that filter.

## 4. Already-known items: status after d591170ed68 and bd6ef320f69

| Known item (GHOST_PERF_AUDIT section 4 / Part C) | Status in HEAD |
|---|---|
| Director: 9 refreshers every frame, 5 unguarded `setText`/frame | **Partly fixed.** Animate header, 4 subject texts and the status strip are `setTextIfChanged` (d591170ed68). Still unguarded: `mActorStyleStatus->setText` (2241, 2317) and `mAnimHint->setText` (2726). `LLUUID::asString()` in draw still present (1427, 1436-1437, 2171, 2180). Cast-list cells were already diffed before Part C. |
| Director: `refreshCastList` re-sets cells every frame | Not actually the case: cells are diffed via `CastRowState` (1534-1581); the per-frame cost is the string building and name lookups, not the writes. |
| Director: `refreshMoveTab`/`refreshPathTab` push selection every frame | **Present**, deliberately (Part C dropped cast revisions); cost 1-3 us. |
| Ghost Studio: 40-90 KB `setText` every 0.5 s, per-frame `refreshDetail` | **Fixed** (bd6ef320f69): revision gates, `isWidgetOnScreen`, 32768 cap, 2 Hz poll only for entity clones, Tracy zones. Two hosts remain by design; the Director host pins 534 px (T3). |
| Switchers: `refreshBank(false)` re-parses the LLSD bank every frame | **Present**; dropped from Part C v3 because setting resets can bypass the signal. |
| Prop Mover: 0.5 s full rebuild of both lists | **Present**; dropped from Part C (paths publicly mutable). |
| Flycam recorder: 2 `setText` + slider writes per frame | **Fixed** (Part C). |
| Lightbox: `getChild` by name per tick, hand-typed tab heights | **Fixed** (Part C caching; 9f31a1f2058 auto-wrap/auto-fit/wheel rule). |
| Cine Light Rig: `gSavedSettings.getXxx(prefix + name)` per frame | **Converted** (Part C cached control arrays); not re-measured here. |
| Path editor tooltips / enables per frame | **Fixed** (Part C). |
| Weather `updateDeferredAvailability` findChild x4 per frame | **Mostly fixed**: pointers cached; two `getBOOL` by name remain (221-222). |
| `LLView::findChildView` 536/frame | Expected to drop after Part C; **unverified** (needs the post-fix capture). No by-name view lookup remains in any Director draw path. |
| Lens Gaze `refreshControls` | Deferred by Part C; not Director-hosted. |

## 5. Fix plan

Order: shared helper and rule first, then per-area. Every item is tagged **safe-mechanical** (behaviour identical except removed redundant work; can ride a Sonnet batch with Codex review), **needs-plan-review** (design choice or shared-state semantics; Codex plan review before code) or **leave-alone** (reason given). Nothing below proposes a revision counter on `LLDirectorCast`, cast groups, `LLActorMover` paths or the switcher banks (Part C "Dropped": publicly mutable or settings-backed).

### 5.1 Shared helper and rule

- **H1. Promote the Lightbox layout helpers to a shared header** (safe-mechanical for the extraction; the Lightbox keeps calling them). Move `lbWrapTabPanel`, `lbFitScrollContainer`, `lbScrolledView`, `lbHasDirectScrollContainer`, `lbCollectTabScrollContainers`, `lbEnableSliderWheel` (alfloaterlightbox.cpp:150-291) into e.g. `indra/newview/alscrollfit.h` as `alWrapTabPanel`, `alFitScrollContainer`, `alCollectTabScrollContainers`, `alEnableSliderWheel`, plus an idempotent `alInstallScrollFocusOnce(root)` that walks all tab-level scroll containers and installs `ALScrollFocus` exactly once per container (the Lightbox's `focus_done` logic, 391-422). Keep the wheel guard (`lbFindWheelTargets`, `lbWheelGuardActive`, `handleScrollWheel/HWheel` 545-606) as a small reusable object (`ALWheelScrollGuard`) that a floater's `handleScrollWheel` delegates to, instead of file statics. Codex already approved the behaviour for the Lightbox; the review here is only "same code, new home".
- **H2. Add a card-fit helper or an `ALFlowGrid` option** (needs-plan-review): either `alFitCardToContent(LLPanel* card)` = title band + lowest child extent (same math as `lbFitScrollContainer`, 185-223) run on every flow-grid card before the grid's first `reshape`, or an `ALFlowGrid` param `fit_cards="true"` that does it inside `reshape`. Review question: the grid is also used by the Cine Light Rig panel (alpanelcinelightrig.cpp:2727-2758 mirrors the grid height onto the wrapper), so the option must default off.
- **H3. Rules for CLAUDE.md "UI PANELS"** (needs-plan-review for wording, then mechanical): (a) no hand-typed scroll-document heights and no `height=` on a `<panel filename=...>` node that embeds a shared panel; every tab panel is wrapped/auto-fitted at build, open and tab change; (b) a shared panel never claims singleton edit state from `draw()`; it claims on focus/visibility with an owner token and releases only what it owns; (c) a refresher that only feeds a tab's widgets is skipped while that tab is hidden and forced on the next show (the Ghost Studio `onVisibilityChange(true)` reset pattern), while refreshers with model side effects run regardless.

### 5.2 Director console, per area

- **D1. Tabs: wrap + auto-fit everything** (safe-mechanical, same code as 9f31a1f2058): in `postBuild`, `alWrapTabPanel` for Ghosts, Props, Takes, Time; `alFitScrollContainer` on all 12 tab-level containers at build, `onOpen` and `onTabChanged`; `alInstallScrollFocusOnce(this)` replacing the four explicit installs (536-551); `alEnableSliderWheel(this)` + the wheel guard in a `handleScrollWheel` override (the Shafts tab has one `wheel_adjust` slider today, l.1643; the Lightbox rule makes all of them consistent). Fixes T1, T9 (effectively), T10. Then delete the seven hand-typed `height=` values on the scroll documents. Note for the Ghosts tab: wrapping a `follows="all"` panel that already scrolls internally would double-scroll; the right move there is to drop the 534 pin and let the panel keep its own 760 inside a tab-level scroll container, or keep follows-all and accept T3; needs a one-line decision.
- **D2. Camera cards: derive card heights** (needs-plan-review, depends on H2): remove the seven hand-typed card heights and the embedded `height=` pins; card = title band + embedded panel's own height. Fixes the overlap half of T2 and the 117-px blank Cinematic card. Also delete the `panel_operator_tuning` stub (panel_cinecam_params.xml:1464) once the fit no longer counts it.
- **D3. Drop the `height=` pins on the other embedded shared panels** (safe-mechanical after D1: path_editor, weather, volumes, temporal, recorder, actor_mover, anim_preview, prop_mover, ghost_studio per the D1 decision) so the file's own root height rules and auto-fit sizes the document. Fixes the clip half of T2.
- **D4. `refreshActorStyleTab`** (safe-mechanical, same inputs, cheaper compare): `mActorStyleStatus->setText` -> `setTextIfChanged` (2241, 2317); replace the roster signature string with a cached `std::vector<RosterRow{LLUUID id; bool in_world; std::string name}>` compared element-wise (no `asString`); replace the 15-field `llformat` snapshot with a stored `LLDirectorCast::ActorStyle` copy and field-wise compare. Behaviour identical: the same inputs decide the same rebuilds. Fixes T5.
- **D5. `refreshAnimateTab`** (safe-mechanical): `mAnimHint->setText` -> `setTextIfChanged` (2726). Cache `pasteAnimId()` keyed on the editor text (recompute only when `mPasteEditor->getText()` differs from the cached string). Fixes T6.
- **D6. `castMemberName` short-id strings** (safe-mechanical): a floater-private `std::map<LLUUID, std::string>` (or `LLUUID::toString` into a reused buffer) for the `substr(0, 8)` fallbacks (1427, 1436-1437) so `asString()` leaves the draw path. Note: `castMemberName` is `static`; make the cache a file-static map cleared on cast membership rebuild (1461-1490), or pass it the floater. No behaviour change.
- **D7. `refreshCastList` / `refreshAnimateTab` small hygiene** (safe-mechanical): cache the 7 column indices in `postBuild` (1492-1496, 2671-2672); iterate the list's items without the `getAllData()` copy if `LLScrollListCtrl` exposes its item list (verify the API first; otherwise leave the copy, it is ~1 us).
- **D8. Hidden-tab gating for pure-UI refreshers** (needs-plan-review): skip `refreshAnimateTab`'s list/row work and `refreshActorStyleTab` when their tab is not in the visible chain, and force a full pass on `onTabChanged`/`onOpen` (dirty flags, like `mHavePrismSummary`). Do **not** gate `refreshMoveTab`, `refreshPathTab`, `refreshGroupControls`, `refreshTransport`, `refreshStatusStrip`: they feed model-side state (mover selection, path target, hotkey behaviour) that must be current while the tab is hidden. Review question: does anything read `mAnimSnapshot`/`mActorStyleShownFor` while hidden (menus, hotkeys)? From this read, no.
- **D9. Tracy zones per refresher** (safe-mechanical, do first so the next capture attributes the 2-5 ms frames): `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("director refreshCastList")` etc. inside each of the nine, and `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` on `ALPanelDirectorSwitcher::draw`, `ALPanelDirectorAnimSwitcher::draw`, `ALPanelPropMover::draw`, `ALPanelAnimPreview::draw`, `ALPanelWeatherSettings::draw`, `ALPanelAtmosphericVolumes::draw`, `ALPanelTemporalCapture::draw`, `ALPanelActorMover::draw` (rule 5; several of these have none).
- **D10. Actor Gaze push on change only** (needs-plan-review): keep the previous `selectedCastIds()` in a member and call `ALFloaterActorGaze::updateSelection` only when it differs or when the gaze floater became visible since the last push. Review question: a gaze floater opened by a toolbar button with source already DIRECTOR would not receive the selection until it changes; the simplest cure is to also push on `ALFloaterActorGaze::onOpen` via `showForSelection`'s path. Fixes T12.
- **D11. Tab persistence by name** (needs-plan-review: `DirectorLastTab` becomes a String setting in settings.xml, with a migration from the old integer): store `getCurrentPanel()->getName()`, restore with `selectTabByName`, fall back to index 0. Fixes T11.
- **D12. Cinecam operator stubs** (safe-mechanical): `getChild` -> `findChild` + null check for `btn_reset_mode` / `btn_reset_all` (alpanelcinecamparams.cpp:396-399) and delete the 1-px stubs (panel_cinecam_operator.xml:389-391). Fixes T8.
- **D13. Settings-by-name in child draws** (safe-mechanical): `LLCachedControl` for `DirectorSwitcherArmed` (alpaneldirectorswitcher.cpp:539), `DirectorAnimSwitcherArmed` (alpaneldirectoranimswitcher.cpp:535), `AlchemyWeatherEnabled` / `AlchemyWeatherLightningEnabled` (alpanelweathersettings.cpp:221-222). Part of T7.
- **D14. Path editor edit-actor ownership** (needs-plan-review, the one real two-host bug, T4): give `LLActorMover::setEditActor` an owner token (`const void* owner`) so a panel can only clear what it set, claim on `onVisibilityChange(true)`/focus/selection change instead of every frame (keep a cheap per-frame re-assert only if the current owner is this panel), and make the hidden panel call `LLPathCamera::stopPreview()` only when it started the preview (`mPreviewOwned`). Review must cover `llfloateractormover.cpp:223` and the path tool (`ALToolPathEdit`?) which may also read the edit actor. Alternative accepted by the house already: the Ghost Studio's `mPlacementOwner` pattern.
- **D15. Switcher bank re-parse** (leave-alone, per Part C v3; optional needs-plan-review): the cheapest safe compare is to keep the last raw `LLSD` returned by `getLLSD` and `llsd_equals` it before normalizing (still a deep walk, ~10-20 us, but no `defaultBank` + 12 strings), falling back to the `ALRevisionGate` 1 Hz self-heal. Only if the Camera tab shows up in the post-fix capture.
- **D16. Prop Mover 0.5 s rebuild** (leave-alone per Part C; one safe-mechanical sliver: `mStatusText->setText` -> `setTextIfChanged` at alpanelpropmover.cpp:205, 210).
- **D17. Anim Preview dummy** (leave-alone: stock Animation Explorer idiom; the 0.3-1 ms is paid only while the Animate tab is visible). If it ever matters: `onVisibilityChange(false)` could drop `mAnimationPreview` and re-create lazily (it already creates lazily, 86-91).
- **D18. `refreshGroupControls` / `refreshTransport` string churn** (leave-alone: Part C's dropped list; the cost is ~30 us and the group model is mutable from many paths).
- **D19. `min_height`** (leave-alone after D1: once every tab scrolls, the chrome minimum is correct).

Suggested batching: D9 + D4 + D5 + D6 + D7 + D12 + D13 + D16 in one Sonnet safe-mechanical batch (Codex review, one build); H1 + D1 + D3 as the second batch (same code as the Lightbox fix, Codex review); H2/D2, D8, D10, D11, D14 each as a reviewed plan before code.

## 6. In-world test checklist (for when the user is back)

Before any fix (confirms the traps as described):
1. Director at default size (1020x650), Props tab: look for the Shuttle row (axis/sign/slope combos, Dist m, m/s, Shuttle) and the "N props enrolled" line drawn over the status strip / below the floater border; try to click "Shuttle". Resize to minimum height; note what disappears. (T1, T9)
2. Ghosts tab: measure how tall each inner sub-tab's scroll viewport is at default and at min height; compare with the standalone Ghost Studio. (T3)
3. Open the Actor Mover floater on its Path tab and the Director on its Path tab with two different actors selected: watch the in-world node highlight for flicker; start a path camera preview from the standalone, then switch the Director to another tab: does the preview stop? (T4)
4. Mouse wheel on every tab: over Shafts sliders (only "Shaft length" should react), over Camera cards (scrolls), inside Ghosts sub-tabs, on Props (expected: nothing scrolls, there is no container). Tab-key through Path / Lights / Weather / Volumes and see whether the focused control scrolls into view (expected: no). (T10)
5. Actor FX tab: change target, toggle Enable, drag a slider; confirm the status line updates and does not flicker (it is rewritten every frame today, so no visible symptom is expected, only Tracy evidence).
6. Animate tab with nothing selected: Tracy should show one `LLTextBase::appendText` per frame from `anim_hint` (T6); select a member with running animations and confirm priority / playing markers update.
7. Camera tab: edit a switcher slot label, load a scene that replaces the bank, confirm both switcher cards refresh within a frame (this is what the per-frame re-parse buys; must still hold after any D15 change).
8. Open the Animation Explorer while the Director's Animate tab is visible: two preview dummies; note frame time. (D17)
9. Reopen the Director after moving Actor FX to another XML position (dev-only): confirms the positional `DirectorLastTab` trap. (T11)
10. Tracy, 20 s per tab with the Director open, after the fixes: Director `LLFloater::draw` p50 should stay near 0.4 ms with p99 under 5 ms (baseline p50 0.41 / p90 4.3 / p99 356 ms); `LLView::findChildView` per frame well below 536; no `LLTextBase::appendText` above 1 ms; the new per-refresher zones attribute the 2-5 ms frames. Re-export with `tracy-csvexport.exe -u -f "director"` into `C:\Users\xianw\AppData\Local\Temp\directoraudit\`.
11. Scene save/load round trip from the transport bar: all tabs and cards intact afterwards; note the one-frame hitch size (every setting signal fires, three `ALPanelCineCamParams` instances run `updateModePanel`).
12. After D1-D3: every tab at default and minimum size shows a scrollbar exactly when content exceeds the viewport; no control is cut or spills; the Weather and Volumes tabs match the Lightbox's versions of the same panels pixel for pixel.

## 7. Files referenced

- `indra/newview/llfloaterdirector.cpp` / `.h`, `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/alpaneldirectorswitcher.cpp`, `alpaneldirectoranimswitcher.cpp`, `aldirectorswitcher.cpp`
- `indra/newview/alpanelghoststudio.cpp`, `alpanelpatheditor.cpp`, `alpanelactormover.cpp`, `alpanelanimpreview.cpp`, `alpanelpropmover.cpp`, `alpanelflycamrecorder.cpp`, `alpanelcinelightrig.cpp`, `alpanelcinecamparams.cpp`, `alpanelflycamorbit.cpp`, `alpanelweathersettings.cpp`, `alpanelatmosphericvolumes.cpp`, `alpaneltemporalcapture.cpp`, `alfloateractorgaze.cpp`, `alpanellensgaze.h`
- `indra/newview/alscrollfocus.h`, `alflowgrid.cpp`, `alfloaterlightbox.cpp` (helpers to promote), `aluirefresh.h`
- `indra/newview/lldirectorcast.cpp`, `llactormover.h`, `llghostavatar.cpp`
- `indra/llui/lltabcontainer.cpp`, `llview.cpp/.h`, `llfloater.cpp`, `llscrolllistctrl.cpp`
- XML: `panel_prop_mover.xml`, `panel_ghost_studio.xml`, `panel_cinecam_params.xml`, `panel_cinecam_operator.xml`, `floater_ghost_studio.xml`, `floater_prop_mover.xml`, `floater_actor_mover.xml`
- Tracy: `captures\alchemy_20261005_103942.csv`, `ghostaudit\floaterdraw_unwrapped.csv`, `doframe_unwrapped.csv`, `appendText_unwrapped.csv`; filtered aggregate written to `directoraudit\aggregate_numeric.csv`.


