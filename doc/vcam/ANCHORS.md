# VCam Monitors design: anchor audit

Spec: `doc/VCAM_MONITORS_DESIGN.md`. Branch `feature/director-kit-p0`, HEAD `68910438e34`.
Paths are repo-relative to `I:\alchemy-machinima`. Snapshot taken about 2026-10-11 18:02 local.

**Snapshot caveat.** Codex was editing `indra/newview/llprismlens.cpp` (mtime 18:01:29) and `llprismlens.h` (mtime 18:00:20) during this audit. `renderCameraGuides` moved from line 7198 to 7439 between two passes, so line numbers in those two files are only as good as the last grep pass. Re-find each item by the symbol in column 3 before relying on a number. At the final check the file had moved again: `renderAuxiliaryView` 7469 to 7878, `getCompositeStates` 7937 to 8347, `displayTargetStatus` 1884 (unchanged). Every `llprismlens.cpp` line past about 2000 in section A is therefore stale by roughly +400 and needs a re-run once Codex stops. Codex has also started VC1 (`displayTargetStatus`, `bindDisplayTarget`, `reassignDisplay`, `setCaptureLabel`, `restoreCaptureIdentity`, `ApplyReport`), so some spec-proposed symbols now exist. They are listed in section B.

Status legend: `unchanged` = same symbol at the same line (within 3). `moved` = same symbol at a different line, or renamed (offset shown). `not found` = symbol absent from the tree.

## A. Spec old references (file:line in the spec)

| spec section | spec's old ref | what it is (symbol) | CURRENT file:line | note |
|---|---|---|---|---|
| 2, b1 | llprismlens.h:27-31 | `MAX_CAPTURES`, `MAX_DISPLAY_BINDINGS` | indra/newview/llprismlens.h:33-34 | moved (+6) |
| 2, b2 | llprismlens.h:477-509 | `CaptureDefinition`, `DisplayDefinition`, `RegistrySnapshot` | llprismlens.h:480, 493, 543 | moved (+3 to +66) |
| 2, b3 | llprismlens.h:365-475 | `ScreenEffects`, `DisplaySettings` | llprismlens.h:376, 449 | moved (+11) |
| 2, b4 | llprismlens.cpp:7719-7757 | TV-effect packing: `ScreenEffects` read and flip/rotate in `getCompositeStates` | llprismlens.cpp:8033 (`getCompositeStates` starts 7937) | moved (+314) |
| 2, b4 | app_settings/shaders/class1/deferred/prismLensF.glsl:64-206 | `main()` and TV effect blocks | indra/newview/app_settings/shaders/class1/deferred/prismLensF.glsl:64 (`main`), 130-320 (TV blocks), 343-345 (sheen) | moved (`main` unchanged at 64; effect blocks now run to about 345) |
| 2, b4 | llprismlens.cpp:7770-7818 | mapping (Fit/Fill/Stretch) and bar colour in `getCompositeStates` | llprismlens.cpp:8088-8123 (FIT 8088, FILL 8106, bar copy 8122) | moved (+318) |
| 2, b5 | pipeline.cpp:1828-1856 | `LLPipeline::allocatePrismLensOutput` (lifetime-fixed RGBA16F output) | indra/newview/pipeline.cpp:1828 (fn), 1840 (output target) | unchanged |
| 2, b6 | llprismlens.cpp:7164-7241 | `renderAuxiliaryView()` | llprismlens.cpp:7469 | moved (+305) |
| 2, b6 | llprismlens.cpp:7248-7259 | exact-size scratch acquire under `ScopedPrismRenderState` | llprismlens.cpp:7547 (scope), 7556 (`acquirePrismLensScratch`) | moved (+308) |
| 2, b6 | llprismlens.cpp:7585-7628 | HDR beauty copied into retained output, then `markProduced` | llprismlens.cpp:7919 (`copyContents`), 7933 (`markProduced`) | moved (+305) |
| 2, b6 | pipeline.cpp:1728-1825 | `LLPipeline::acquirePrismLensScratch` (8-entry pool) | indra/newview/pipeline.cpp:1728 (fn); pool loop 1763-1790 | unchanged |
| 2, b7 | llprismlens.cpp:2876-2886 | no-visible-display exit and Gate prewarm in `prepareCameraCapture` (`mAnyDisplayVisible`) | llprismlens.cpp:3076-3086 | moved (+200) |
| 2, b8 | llprismlens.cpp:3041-3156 | `PrismLensRegistry::updateCadenceEntitlements` (water-fill) | llprismlens.cpp:3241 | moved (+200) |
| 2, b8 | llprismlens.cpp:3159-3249 | `PrismLensRegistry::chooseRenderSlot` (sole admission point) | llprismlens.cpp:3359 | moved (+200) |
| 2, b9 | llprismlens.cpp:259-523 | `PrismAdaptiveController` | llprismlens.cpp:283 (class), 286 (`updateFrame`) | moved (+24) |
| 2, b10 | pipeline.cpp:26581-26838 | prism composite draw block; binds `mPrismLensOutput` | indra/newview/pipeline.cpp:26603 (block start), 26828-26837 (retained-output bind) | moved (+22) |
| 2, b10 | llprismlens.cpp:7632-7861 | `getCompositeStates` | llprismlens.cpp:7937 | moved (+305) |
| 2, b10 | llprismlens.cpp:7895-8076 | `getAuxCompositeStates` | llprismlens.cpp:8200 | moved (+305) |
| 2, b11 | llfloaterprismmanager.cpp:1778-2425 | manager add, remove, and commit handlers | indra/newview/llfloaterprismmanager.cpp:1782 (`onAddCamera`) through 2390 (`onApplyEffectsPreset`) | unchanged |
| 2, b12 | llfloaterprismmanager.cpp:705-777 | `onOpen`, `draw()`, `pollSnapshots` (0.25 s poll) | llfloaterprismmanager.cpp:705, 715, 722 | unchanged |
| 2, b12 | llfloaterprismmanager.cpp:953-1060 | `rebuildCaptureList`, `rebuildDisplayList` | llfloaterprismmanager.cpp:954, 1004 | unchanged |
| 2, b13 | llfloaterdirector.cpp:3408-3533 | `LLFloaterDirector::refreshPrismDependents` (VCam summary poll) | indra/newview/llfloaterdirector.cpp:4292 (fn), revision compare 4379-4384 | moved (+884) |
| 2, b14 | floater_director.xml:1342-1353 | v1 VCam tab (`vcam_tab`, `lens_vcam_card`, `prism_summary`) | indra/newview/skins/default/xui/en/floater_director.xml:1342, 1345, 1348 | unchanged |
| 2, b15 | alvcambonepov.h:20-27 | `ALVCamBonePov` class, `status()` | indra/newview/alvcambonepov.h:20, 27 | unchanged |
| 2, b16 | llprismlens.cpp:1177-1275 | `selectedFaceIdentity` (LLSelectMgr face) | llprismlens.cpp:1201 | moved (+24) |
| 2, b16 | llprismlens.cpp:1244-1263 | HUD and attachment reject; rigged/animated reject in selection path | llprismlens.cpp:1263, 1283 | moved (+19 / +20) |
| 2, b16 | llprismlens.cpp:1825-1881 | `displaySelectionStatus` | llprismlens.cpp:1869 (`displaySelectionStatus`, now delegates at 1881 to `displayTargetStatus`), 1884 (`displayTargetStatus`) | moved (+44) |
| 2, b16 | llprismlens.cpp:2017-2058 | "`addSelectedDisplay`" as the spec names it | llprismlens.cpp:2053 (`addLens`, surface-lens creation), 2107 (`addDisplay`), 2131 (`bindDisplayTarget`), namespace `addSelectedDisplay` at 7064 | moved; the cited range now holds `addLens`, not display creation. Confirm intent. |
| 2, b17 | llprismlens.h:580-581 | "viewer-local, no prim/TE/material write" comment | llprismlens.h:623 | moved (+43) |
| 2, b18 | indra/llui/llui.h:54-82 | `EDragAndDropType` (`DAD_*`); `DAD_COUNT = 22` | indra/llui/llui.h:56-81 | unchanged |
| 2, b18 | lltooldraganddrop.cpp:211-236 | `LLDragAndDropDictionary` ctor (3D dispatch table) | indra/newview/lltooldraganddrop.cpp:211 | unchanged |
| 2, b18 | lltooldraganddrop.cpp:499-620 | `dragOrDrop`; `is_uuid_dragged` (SOURCE_PEOPLE only) | lltooldraganddrop.cpp:499 (fn), 521 (`is_uuid_dragged`), branches 545, 588, 628, 668 | unchanged (start) |
| 2, b16 | llprismlens.cpp:2617-2721 | `prepareDisplayFrame` | llprismlens.cpp:2792 | moved (+175) |
| 3.1 | settings.xml:29432-29438 | `PrismLensEnabled` | indra/newview/app_settings/settings.xml:29476 | moved (+44) |
| 3.1 | settings.xml:29474-29480 | `PrismLensZoom` | settings.xml:29518 | moved (+44) |
| 3.1 | settings.xml:29502-29508 | `PrismAdaptivePerformance` | settings.xml:29546 | moved (+44) |
| 3.1 | settings.xml:29509-29515 | `PrismProtectedMainFPS` | settings.xml:29553 | moved (+44) |
| 3.1 | settings.xml:29516-29521 | `PrismCaptureRefreshCeilingHz` | settings.xml:29560 | moved (+44) |
| 3.1 | settings.xml:29481-29487 | `PrismLensResolutionScale` | settings.xml:29525 | moved (+44) |
| 3.1 | llprismlens.cpp:3515-3556 | `setCameraSettings` (registry member) | llprismlens.cpp:3715 (member), 7084 (namespace wrapper); header 670 | moved (+200) |
| 3.1 | llprismlens.cpp:3641-3667 | `setCaptureRateSettings()` | member is now `setRateSettings` at llprismlens.cpp:3841; namespace wrapper 7107; header 688 | moved (renamed member) |
| 3.1 | llprismlens.cpp:3670-3746 | `setDisplaySettings` | llprismlens.cpp:3954 (member), 7114 (wrapper); header 693 | moved (+284) |
| 3.1 | llprismlens.h:650-653 | `setDisplayGateSubscribed` | llprismlens.h:705; cpp member 4356; wrapper 7141 | moved (+55) |
| 4.2 | llprismlens.cpp:2781-2805 | `prepare()` reset and recompute of `mAnyDisplayVisible` | llprismlens.cpp:2981, 2999, 3005 (in `prepareCameraCapture`) | moved (+200) |
| 4.2 | llprismlens.cpp:2774-2921 | `prepareCameraCapture` | llprismlens.cpp:2974 | moved (+200) |
| 4.2 | llprismlens.cpp:7174-7219 | adaptive `updateFrame` and visible-lens `prepare` loop, before `chooseRenderSlot` | llprismlens.cpp:7481 (`updateFrame`), 7515-7518 (prepare loop), 7526 (`chooseRenderSlot`) | moved (+305) |
| 4.2 | llprismlens.cpp:3096-3122 | water-fill entitlement loop | llprismlens.cpp:3296 (comment), loop 3296-3330 | moved (+200) |
| 4.2 | llprismlens.cpp:3185-3197 | automatic-producer break | llprismlens.cpp:3396 (`break` in the every-frame branch, 3385-3397) | moved (+211) |
| 4.2 | llprismlens.cpp:3252 | `markProduced` | llprismlens.cpp:3452 | moved (+200) |
| 4.2 | llviewerdisplay.cpp:937-939 | `LLPrismLens::renderAuxiliaryView()` call before the main view | indra/newview/llviewerdisplay.cpp:939 | unchanged |
| 4.2 | llprismlens.cpp:7240-7259 | `"Prism one scheduled auxiliary update"` zone and `ScopedPrismRenderState` | llprismlens.cpp:7545 (zone), 7547 (scope) | moved (+305) |
| 4.2 | llprismlens.cpp:7322-7345 | camera-feed aux far clip | llprismlens.cpp:7628-7649 | moved (+306) |
| 4.2 | llprismlens.cpp:7378-7430 | surface-lens aux far clip | llprismlens.cpp:7684-7733 | moved (+306) |
| 4.2 | llprismlens.cpp:7504-7519 | sun-cascade re-express and `RenderShadowDetail` gates | llprismlens.cpp:7810 (`PrismSunCascadeReexpress`), 7824 (`RenderShadowDetail > 1`) | moved (+306) |
| 4.2 | llprismlens.cpp:7504-7519 (spot block) | `generatePrismSpotShadows` call | llprismlens.cpp:7868 | moved (+364) |
| 4.2 | pipeline.cpp:25523-25579 | deferred SSAO dispatch and blur gate | indra/newview/pipeline.cpp:25586 | moved (+63) |
| 4.2 | pipeline.cpp:1752-1759 | pack-completeness predicate (`pack_complete` lambda) | pipeline.cpp:1753 | unchanged |
| 4.2 | pipeline.cpp:18933-18961 | directional volumetrics (`renderVolumetric`) | pipeline.cpp:18933 (fn), 18935 (gate) | unchanged |
| 4.2 | pipeline.cpp:20896-20901 | projector volumetrics gate | pipeline.cpp:20896-20901 (fn `renderProjectorVolumetric` at 20850) | unchanged |
| 4.2 | llreflectionmapmanager.cpp:260 | `LLReflectionMapManager::update()` | indra/newview/llreflectionmapmanager.cpp:260 | unchanged |
| 4.2 | llviewerdisplay.cpp:1200-1206 | probe update, "once per presented frame" | indra/newview/llviewerdisplay.cpp:1204, inside the `probe_before_present` pre-swap block (1179-1205) | unchanged location. Semantics differ from the spec's wording. Verify. |
| 4.2 | pipeline.cpp:26747-26754 | `gFrameTimeSeconds` for animated effects | pipeline.cpp:26776-26783 | moved (+29) |
| 4.2 | llglslshader.cpp:261-292 | `GL_TIME_ELAPSED` profile query | indra/llrender/llglslshader.cpp:261 (`placeProfileQuery`), 282 (`readProfileQuery`) | unchanged |
| 4.3 | llprismlens.cpp:7763-7768 | logical output region transform | llprismlens.cpp:8068-8073 (`getCompositeStates`), 8319-8324 (`getAuxCompositeStates`) | moved (+305) |
| 4.3 | llprismlens.cpp:7779-7816 | camera-feed-only mapping (Surface Lens invariant) | llprismlens.cpp:8088-8125 | moved (+309) |
| 4.3 | pipeline.cpp:12833-12888 | `LLPipeline::colorCorrect` eight-way permutation selection | indra/newview/pipeline.cpp:12833 (fn) | unchanged |
| 4.3 | pipeline.cpp:12886-12907 | bloom pyramid bind inside `colorCorrect` | pipeline.cpp:12886-12909 | unchanged |
| 5.1 | llinventorypanel.h:493 | `mDragTypes[DAD_COUNT]` | indra/newview/llinventorypanel.h:493 | unchanged |
| 5.1 | lltooldraganddrop.h:61-68 | `LLToolDragAndDrop::ESource` | indra/newview/lltooldraganddrop.h:61-68 (`SOURCE_VIEWER` 67, `SOURCE_PEOPLE` 68) | unchanged |
| 5.1 | lltooldraganddrop.cpp:3371-3374 | `locateInventory` SOURCE_VIEWER branch | lltooldraganddrop.cpp:3327 (fn), 3371 (branch) | unchanged |
| 5.1 | lltoolbarview.cpp:949-965 | `resetDragTool`, `mDragItem` | indra/newview/lltoolbarview.cpp:949, 963 | unchanged |
| 5.1 | lltooldraganddrop.cpp:573-595 | top-view second pass for people cargo; `drop=false` bug | lltooldraganddrop.cpp:579-595; the `drop=false` call is at 590 | moved (+6). Bug still present. |
| 5.2 | llprismlens.cpp:1868-1877 | rigged, HUD, dead, and `permModify` checks | llprismlens.cpp:1905 (HUD/dead), 1915 (rigged/animated), 1923 (`permModify`); all in `displayTargetStatus` | moved (+37 to +46) |
| 5.2 | llprismlens.cpp:1862-1866 | source-object feedback rule | llprismlens.cpp:1950 | moved (+88) |
| 5.2 | llprismlens.cpp:1836-1845 | Surface Lens one-display rule and 16-face cap | llprismlens.cpp:1836 (16-face cap, `lensSelectionStatus`), 1899 (one-display text, `displayTargetStatus`), 2190 (reassign path) | moved (+64) |
| 5.2 | llprismlens.cpp:4472-4477 | Surface Lens display removal refused | llprismlens.cpp:4702 (`removeDisplay`) | moved (+230) |
| 5.2 | llfloaterdirector.cpp:1478-1494 | `loadScene` Prism all-or-nothing early return | indra/newview/llfloaterdirector.cpp:2340-2347 (in `loadScene`, 2296) | moved (+862) |
| 5.2 | llfloaterdirector.cpp:1520-1664 | Director apply after the Prism apply | llfloaterdirector.cpp:2296 (`loadScene`), 2375 (`cast.applySceneData`), 2519 (light-rig apply) | moved (apply block now about 2350-2600) |
| 5.4 | llprismlens.cpp:5503-5579 | `applySceneData` commit path (`releasePrismLensBuffers`, resets) | member `applySceneData` now llprismlens.cpp:5368; `releasePrismLensBuffers` call at 5811 (last pass) | moved. Line 5811 may shift again with Codex edits. |
| 5.4 | llprismlens.cpp:6031-6037 | `clearSlot` detaches Gate-subscribed displays | llprismlens.cpp:6334-6348 (display loop in `clearSlot`, 6317) | moved (+303) |
| 5.4 | llprismlens.cpp:6045-6057 | `clearSlot` disarm fixup | llprismlens.cpp:6354-6361 (`gateDisarm` call at 6358) | moved (+309) |
| 6.3 | llprismlens.h:617-621 | `setSelectedCamera` | llprismlens.h:670; namespace wrapper llprismlens.cpp:7079 | moved (+53) |
| 6.3 | llfloaterprismmanager.cpp:1907-1983 | "Snap to my view" current-view transform | llfloaterprismmanager.cpp:1926 (`prismCurrentViewTransform`), 1941 (`onSnapVirtualCameraToView`) | unchanged |
| 6.3 | llfloaterprismmanager.cpp:279-319 | screen-effect presets | llfloaterprismmanager.cpp:279 (CRT), 289 (BrokenTV), 301 (VHS), 311 (CCTV) | unchanged |
| 6.1 | llfloaterdirector.cpp:809-821 | `selectLeafTab` routing and `migrate_tab_name` | llfloaterdirector.cpp:1505 (`selectLeafTab`), 99 (`migrate_tab_name`), 1441-1452 (`DirectorLastTabName` restore) | moved (+696) |

Section A counts (85 spec old references): **unchanged 25, moved 60, not found 0.**

## B. Spec-proposed symbols (no old line in the spec)

These are new APIs the spec proposes. Some already exist as Codex VC1 work in progress.

| spec section | symbol | CURRENT location | note |
|---|---|---|---|
| 3.2 | `CaptureDefinition::mLabel` | llprismlens.h:484 | found (new, VC1 in progress) |
| 3.2 | `setCaptureLabel` | llprismlens.h:691; wrapper llprismlens.cpp:7460; registry `setLabel` at cpp:3870 | found (new). Spec's name is kept on the wrapper. |
| 3.2 | `PrismMonitorPreviewsEnabled`, `PrismMonitorPreviewFPS` | settings.xml | not found (planned) |
| 3.2 | `mSelectedCapture`, `mMonitorDisplay`, `mSection` | none | not found (planned; no page model yet) |
| 3.3 | `ALVCamPageModel` (`alvcampagemodel.h/.cpp`) | none | not found (file absent) |
| 3.3 | `ALKitHistory` (`alkithistory.h/.cpp`) | indra/newview/alkithistory.h | found (existing) |
| 4.1 | `LLPrismPreview` (`llprismpreview.h/.cpp`) | none | not found |
| 4.2 | `PrismInstance::mPreviewDemanded`, `mPreviewOnly`, `mNextPreviewDueTime`, `mPublicationSerial` | none | not found |
| 4.2 | `ScopedPrismRenderState` (existing scope the preview scope will sit beside) | llprismlens.cpp:6583 (ctor 6585, dtor 6629) | found (existing) |
| 4.2 | `LLPipeline::beginPrismAuxiliaryState` / `endPrismAuxiliaryState` | pipeline.cpp:10683 / 10857 | found (existing) |
| 4.2 | `sPrismPreviewRender`, `ScopedPrismPreviewQuality` | none | not found |
| 4.3 | `buildSignalCompositeState`, `SignalCompositeState` | none. The duplicated math is in `getCompositeStates` (llprismlens.cpp:8033-8125) and `getAuxCompositeStates` (8286-8374). | not found |
| 4.3 | `prismSignalF.glsl`, `prismSignalSample`, `prismPreviewF.glsl` | none | not found |
| 4.4 | `ALVCamMonitorTile`, `ALVCamMonitorStrip`, `ALPanelDirectorVCam` | none | not found |
| 4.5 | `allocatePrismPreviewScratch`, `mPrismPreviewScratch` | none | not found |
| 4.5 | `PrismPreviewSelfTest`, `LLPrismPreview::runSelfTest` | none | not found |
| 5.1 | `DAD_VCAM_FEED`, `SOURCE_VCAM`, `dad3dVCamFeed` | none (`DAD_COUNT` is 22 at llui.h:81) | not found |
| 5.2 | `displayTargetStatus`, `bindDisplayTarget`, `reassignDisplay` | llprismlens.h:628, 653, 657; cpp 1884, 2131, 2175 | found (VC1 in progress) |
| 5.2 | `ApplyReport` | llprismlens.h:514; member `applySceneData` llprismlens.cpp:5368 | found (new). The "Scene loaded; N ... skipped" summary is not found. |
| 5.2 | `ERestorePolicy` | none | not checked in this pass |
| 5.4 | `RemovedCaptureRecord`, `restoreCaptureIdentity` | llprismlens.h:527, 719; cpp 4846; wrapper 7540 | found (new) |
| 5.4 | `gateRearm` | none | not found |

## C. Summary and risks

- Section A (unchanged 25, moved 60, not found 0): no spec-cited symbol is gone. Every old reference still resolves, and the "not found" items are all proposed new code (section B). The risk is the spec's **old-range mislabels**: the `addSelectedDisplay` range (2017-2058) now holds `addLens`, and `setCaptureRateSettings` is now `setRateSettings` in the registry.
- Two semantic checks, not just line checks: the probe update at `llviewerdisplay.cpp:1204` is in the pre-swap `probe_before_present` block, and the bloom and TV-effect composite code is duplicated between `getCompositeStates` and `getAuxCompositeStates`.
- `llprismlens.*` numbers will keep moving while Codex works. Re-run the symbol search before quoting any number from column 4.
