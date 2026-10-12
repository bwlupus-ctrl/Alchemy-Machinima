# VC6 — Director v2 Camera > VCam

## Implemented

- The generated v2 VCam page is now a native kit page with 126 inventory items (the page master plus 125 indexed rows), five signal-chain steps, scope badges, conditional rows, typed combos/segments, search/help/chips, and no history bindings (`scripts/code_tools/director_kit/vcam_page.json:1`, `scripts/code_tools/director_kit/kit_pages.json:4`, `scripts/code_tools/director_kit_gen.py:395`).
- `ALPanelDirectorVCam` owns the once-built control cache, model/settings signals, add/remove and row commits, conditional visibility, monitor selection/menu actions, and page-only preview demand (`indra/newview/alpaneldirectorvcam.cpp:151`, `:274`, `:291`, `:625`). It deliberately creates no `ALKitHistory` gesture or entry.
- `ALVCamPageModel` coalesces Prism registry/runtime/performance/selection/preview revisions and retains dirty state while hidden; a queued idle callback also exits without snapshot work after the page hides (`indra/newview/alvcampagemodel.cpp:43`, `:72`, `:87`, `:96`).
- `ALVCamMonitorStrip` keeps VC5's drag/drop source and adds the fixed atlas-backed monitor UI, pager, camera chips, and face drop list. Draw paths consume only cached text/layout/state plus the atlas bind (`indra/newview/alvcammonitorstrip.cpp:77`, `:106`, `:181`, `:267`, `:296`, `:340`).
- The shared Director adapter no longer binds VCam through the old Camera binder. Qualified old/new persisted names migrate to the unchanged `vcam_tab` leaf, preserving name routing and the shell's existing rect/tab carry-over (`indra/newview/llfloaterdirector.cpp:99`, `:948`).
- Functional-manifest generation imports the VCam generator source directly, covering master, previews, speed, layout segments, selector/add/remove, all five step chips, and every interactive row (`scripts/code_tools/director_functional_manifest.py:929`).

## Generator extensions

- Added native `segmented`/`radio_group` output, explicit item values (`iv`) for value-typed combos and segments, custom XUI tags, initially-hidden conditional rows, explicit scope badges, and `no_history` pages (`scripts/code_tools/director_kit_gen.py:50`, `:442`, `:473`, `:516`, `:582`, `:592`, `:875`).
- Added custom row tags/defaults for the monitor header/strip/tile/pager, camera selector, and face list (`indra/newview/skins/default/xui/en/widgets/vcam_*.xml`).
- Generated `panel_dpage_camera_vcam.xml` and the 125 VCam index rows. Other generated page payloads are byte-identical; only their generated provenance comment changed.

## All 27 UNDEFINED IN SPEC decisions

1. **MON-FOOT** — derived, read-only footer from preview enable/rate, capture count, and whether cameras already have visible faces.
2. **SRC-TYPE** — clearly read-only source label; no in-place conversion API exists.
3. **SRC-NOTE-OBJ** — read-only mockup help copy.
4. **SRC-NOTE-VIRT** — read-only mockup help copy.
5. **SRC-NOTE-SKEL** — read-only mockup help copy.
6. **SRC-LENS-NOTE** — read-only mockup help copy.
7. **PL-FRONT** — clearly labelled info-only row; no safe geometry-derived placement API exists.
8. **PL-NOTE** — read-only mockup help copy.
9. **PL-NOTE-V** — read-only mockup help copy.
10. **LN-EQ** — derived readout from the effective vertical FOV and a 24 mm sensor height.
11. **LN-FROMPROJ** — derived readout from `CaptureRuntimeState::mEffectiveVerticalFovRad`.
12. **LN-ZOOM-NOTE** — read-only mockup help copy.
13. **GD-NOTE** — read-only mockup help copy shown while the guide is off.
14. **LK-FEEDS** — derived readout from the selected capture's display count.
15. **LK-NOTE** — read-only mockup help copy.
16. **LK-GOOUT** — bound to kit chip navigation (`setChipIndex(3)`).
17. **OF-CUSTOM** — writes the existing `CameraSettings::mOutputAspect`; Custom reveals the same stored ratio editor rather than adding storage.
18. **OF-PIC** — derived logical-size/quality readout from output aspect and the performance snapshot.
19. **MP-SHAPE** — derived from display kind, dimensions/face number, and runtime visibility.
20. **MP-GATE-NOTE** — read-only mockup help copy, visible only for a Gate-subscribed face.
21. **RAW-NOTE** — read-only mockup help copy, visible when the monitor edits raw output.
22. **RT-PRESET** — writes the existing target-FPS value and TARGET_FPS mode, including 23.976 and 29.97 typed values.
23. **RT-NOTE** — read-only mockup help copy.
24. **PF-ADMIT** — derived from admitted global attempt rate and effective total budget.
25. **PF-NOTE-1** — read-only mockup help copy.
26. **PF-NOTE-2** — read-only mockup help copy.
27. **PF-NOTE-3** — read-only mockup help copy.

## Mockup/spec resolutions and deviations

- The mockup wins for card/step order, text, camera-first selector behavior, Large/Compact controls, tile badges, raw/face monitor choices, and the absence of undo/redo.
- Tile state mapping is: LIVE→LIVE, PREVIEW/READY→PREVIEW, PAUSED/HELD→PAUSED, THROTTLED→THROTTLED, OFFLINE/FAILED/SHADER_UNAVAILABLE→OFFLINE, EMPTY/WARMING→WARMING.
- Monitor clicks start the VC5 drag source (or open the ellipsis menu) and never select the edited camera; only Settings-for chips or the explicit **Edit settings** menu action do so.
- The mockup's unsupported **Place eye in front** action is the only action converted to an info row. Source type is likewise read-only because conversion semantics are undefined.

## Verification

- No build was run. XML parsing (page, index, floater, menu, and five widget defaults), JSON parsing, and `git diff --check` pass.
- The checked-in Python launcher is unavailable in this sandbox (`Access is denied`), so the Python `--check`, `ui_draw_lint`, `xui_layout_check`, and functional-manifest emit commands could not be executed here. The draw roots contain only profiling, cached rendering, and base draw calls; generated page content was independently emitted from the same JSON contract.

## Fix round

- Explicit-height/custom kit rows now preserve their declared height, restoring the full 170 px monitor strip and 150 px face-list hit area. Generated segmented radio items have explicit horizontal geometry and are also reflowed evenly at runtime.
- Preview demand is owner-scoped and diffed by capture/selection. The VCam page releases only its own demand on hide/destruction; ordinary edits and selection/manipulator updates preserve publications and serials.
- Large monitor mode keeps four tiles plus the 1–4/5–8 pager; Compact shows all eight. Tiles now use publication timestamps, PAUSED when previews are off, editing/rate badges, status dots, and a drag handle; dragging begins only after mouse slop.
- Action feedback has its own six-second status row. Invalid surface binds and Surface Lens drag attempts show their reason, and the tile menu now has bind/move/cancel plus explicit raw/bound-face display choices at the clicked ellipsis.
- Camera and face empty states hide inapplicable cards. Source is a real Object/Virtual/Skeleton POV selector, Bone POV has an enable control, and reset never enables attachment on a non-virtual camera.

## Fix round 2

- Kit card and row code visibility is now persistent layout state and is combined with mode, chip, search, advanced, and branch filtering on every relayout. VCam conditional rows use that API, and unchanged card labels/visibility no longer schedule full-page layout work.
- The face list now wheel-scrolls with a visible scrollbar and offset-aware selection/drop hit testing. Segmented rows draw and click as button segments, Settings for is one selector/action row, monitor drags use the viewer drag threshold, and rejected-drag tooltips clear on mouse-up.
- Non-producing cameras read OFFLINE. Move existing face now enters a cancellable face-pick state and reassigns the chosen display through the existing validated registry path.
- Face selection is authoritative for the editors and list highlight, while monitor display choice remains independent. Face names use one global numbering scheme; aspect combos select presets by tolerance and fall back to Custom.
- TV presets now match the full manager values. Dynamic card badges use the real camera slot/label, and the monitor menu is destroyed with its owning panel.
- Added isolated migration coverage for qualified VCam names and `DirectorLastTabName`/legacy-index restoration. No build was run in this fix round, per request.
