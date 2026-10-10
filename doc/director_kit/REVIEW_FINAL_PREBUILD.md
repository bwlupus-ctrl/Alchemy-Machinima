# Final pre-build review: 9c25f75491a..HEAD (feature/director-kit-p0, tip e12eba2efda)

**Verdict: BUILD.** I found no BLOCKER or MAJOR issues. There are three MINOR findings. None of them affects whether the build links or starts.

Scope: the cross-cutting, link and startup risks that the per-WP reviews and `/Zs` cannot see. The per-WP logic was not re-reviewed.

## Findings

### MINOR-1: the pointer hook's fallback hit test never reaches a Director view, and it runs on every click
- `llviewerwindow.cpp:1138-1140`: when no top control contains the click, the hook calls `mRootView->childFromPoint(x,y,true)`.
- `LLView::childFromPoint` (`llview.cpp:922-951`) returns the frontmost *visible* child that contains the point. It ignores `mouse_opaque`.
- In `main_view.xml:159-170`, the last children are the full-window `Menu Holder` and `tooltip view`. `addChild` pushes to the front, so `tooltip view` is frontmost. `gConsole` is added to root after `main_view` (`llviewerwindow.cpp:2561`).
- Result: `hit` is always `tooltip view` or the console, never a Director view. `ALKitHistory::notePointer` (`alkithistory.cpp:75-79`) only fires through the top-ctrl branch (combo lists).
- No functional loss was found:
  - In-floater clicks still start gestures through `LLFloaterDirector::handleMouseDown`, `handleRightMouseDown` and `handleScrollWheel`.
  - The kit menus call `beginGesture` explicitly (`alkitcard.cpp:301,328`, `alkitpresets.cpp:326`, `alkitpage.cpp:960`, `allightrigmap.cpp:534`).
- Cost: one full-depth `childFromPoint` on every mouse down and up. This happens with v1 too, and with no Director open. It is cheap, but it is dead work.
- Fix (post-build is fine): drop the fallback and keep only the top-ctrl test, or hit-test from `gFloaterView`.

### MINOR-2: Part 3 of the in-world checklist says nothing changes with v2 off, which is no longer true
`doc/INWORLD_CHECKLIST_NEXT_BUILD.md:79-89` was written at cb914c2f03f, before WP1-WP4. With `DirectorUIV2` = 0, the following v1 behaviour has changed:
- **Object-target Mirror.** Facing is now set from the object's root rotation (`alcinelightrig.cpp:2517-2521`), so a mirrored, object-targeted rig mirrors about the object's front instead of world east. This was intended (USER_DECISIONS item 4), but it is not on the checklist.
- **Compass dial wheel.** Wheel events on the v1 dials (`panel_actor_mover.xml:65`, `panel_ghost_studio.xml:78`) now pass through `LLSliderWheel::guarded` (`alcompassdial.cpp:231`). During a page scroll-through, the wheel scrolls the page instead of trimming the dial. Checklist item 2 tests drag and arrow keys only.
- **Light Rig refactor.** The v1 Light Rig Easy/Advanced toggle and Rig Rim preset, status and Tron rows now run through new code: `applyEasyMode` and `syncEasyModeState` (`alpanelcinelightrig.cpp:2104,2122`), and `ALRigRimController` (`:690,2726`). Checklist item 3 only checks card layout.
- Fix: add the following to Part 3:
  - Object target plus Mirror on a rotated prop.
  - Wheel over the Actor Mover dial while the page is scrolling, and while it is idle.
  - Light Rig Easy toggle on and off.
  - Rig Rim preset apply. The combo should snap back to "Choose preset...".
  - The Tron rows follow Rig Rim Enable.

### MINOR-3: `ModeAdapterEntry` has external linkage
- `alkitbinding.cpp:305` declares `struct ModeAdapterEntry` at global scope. It is outside the anonymous namespace at `:9-44`.
- The name is unique in `indra/` today, so it causes no ODR violation now. A future struct with the same name in another TU would be a silent ODR clash under LTCG.
- Fix: move it into the anonymous namespace. This is not build-blocking.

## Verified clean (evidence)

**1. Link**
- **Every new source and header is listed exactly once in `CMakeLists.txt` and exists on disk.** That is 17 new `.cpp` files plus `alkitmath.h` (`CMakeLists.txt:200-216`, `1108-1125`).
- **TUT test registered correctly.** The registration is `LL_ADD_INTEGRATION_TEST(alkitmath "" ...)` (`:2821`), the same header-only pattern as `alxinputmath`. Tests 1-19 are unique, with a single group, `gAlKitMath`.
- **Every member function declared in the new or changed headers has a definition.** I checked this by script against every `.cpp` in newview and llui. The script's only hits were LLViewerRegion inline-in-header accessors, which are false positives.
- **No static data members need out-of-line definitions.** The only one is `MAX_STEPS`, which is `static constexpr`.
- **No ODR duplicates.**
  - Every class defined in a `.cpp` is either in an anonymous namespace (alkitpage, alkitcard, alkitrow, alkitshell, allightrigmap, alpanelrigrim, alrigrimcontroller, aluiperfselftest `59..3370`), function-local, or unique (MINOR-3).
  - The `RigRimPresetRow` table moved from alpanelcinelightrig to alrigrimcontroller. Both copies are in anonymous namespaces, and the old one is deleted.
  - Every `alkitmath.h` function is `inline`.
- **No unity build**, so anonymous-namespace collisions are impossible.

**2. Startup and static init**
- **Widget tags are unique, and each registered class has its own `Params` block.** The tags are 15 `kit_*`, `light_rig_map` and `flow_grid` (the last is pre-existing). Notably, `ALKitScroller::Params` is `Block<Params, LLScrollContainer::Params>`, so the earlier null-deref bug's Params reuse is not repeated.
- **`panel_rig_rim` is a unique injector name.** The `ALKitPresetRegistry` singleton is touched only in the panel constructor and `postBuild`, never at static init.
- **No namespace-scope `LLCachedControl` or singleton use at static init.** Every `LLCachedControl` is function-local.
- **`registerFloaters` reads `DirectorUIV2` after the settings are loaded.** `initConfiguration` runs at `llappviewer.cpp:885` and `registerFloaters` at `:1025`.
- **Settings.** All 13 new settings are present once, with the correct type and the following persist flags:
  - Persisted: `DirectorV2*`, `KitShowUIMs`, `KitSearchGlobal`.
  - Session-only (Persist 0): `UILayoutAuditSizes`, `UIPerfDirectorV2`, `RenderWaterLevel`, `RenderWaterLevelOverride`.
- **Every string-literal setting read by the changed TUs exists with a matching type.** I checked this by script. Variable-name reads (`CineLightRig{Key,Fill,Rim,Bg}{Yaw,Pitch,On,OffsetZ}` and the 39 `CineRigRim*` names) all exist.
- **Every `control_name` and `*setting*` attribute in `floater_director_v2.xml` and all `panel_dpage_*.xml` files resolves.** The only misses were three `water_*` names in `panel_settings_water.xml`, which were already there before this range.
- **Resources resolve.**
  - All 36 `Kit*` colours are in `colors.xml`.
  - The four kit PNGs (16-18 px) exist, and `viewer_manifest.py:183` (`*/*.png`) packages them.
  - `IBMPlexMono` is in `fonts.xml:325`.
  - Every name and column that `bindChrome` needs with `getChild` is present in `floater_director_v2.xml`. The `anim_list` `prio` and `playing` columns are present in the animate page.

**3. v1 safety with `DirectorUIV2` = 0**
- **Director.**
  - The `postBuild`, `draw`, key, mouse and wheel overrides all gate on `mIsV2` or `mShell`.
  - `hasAccelerators` returns false, which equals the base.
  - There is one extra `findChild("kit_ui_ms")` at `postBuild`.
- **Light Rig per-tick cost.** `tickShared` gains three POD copies, about 400 bytes with no heap use (`alcinelightrigmodel.h:140-169`). The map frame is computed lazily, only on demand.
- **Water.** `updateRegions` checks a single bool each frame.
- **Key hook.** The `handleKey` hook is a single timer store.
- **Compass dial.** The `onCommit` override is a no-op for the v1 dials, because they have no `control_name`.
- **Behaviour changes.** See MINOR-2.

**4. Package interactions**
- **Water and kit.** Water (02422bd48fd) and the kit share no files or settings.
- **Self-test.** The self-test inventory path is unchanged. Only the constructor signature changed. The default audit sizes still parse to default and minimum, and "not resizable" still ends in `AUDIT_RESTORE`.
- **Gesture timer ownership.** `GestureTimer` ownership is correct. It calls `release()` before `tick()` returns true (`alkithistory.cpp:222,234`), and `LLEventTimer::updateClass` deletes the timer (`lleventtimer.cpp:63-66`).

**5. CMake**
- No case or name mismatches.
- No source is listed twice.
- The test target is inside `BUILD_TESTING`, next to its precedent.
