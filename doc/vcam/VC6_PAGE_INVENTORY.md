# VC6 Page Inventory: Director Camera > VCam

Input for work package VC6 (`doc/VCAM_MONITORS_DESIGN.md` section 8). Read-only pass; no code, data, or generator file was changed.

Sources read:
- `doc/VCAM_MONITORS_DESIGN.md` sections 1, 2, 3, 4.1-4.5, 5, 6.1-6.4, 7, 8 (VC6 acceptance).
- `doc/director_kit/mockup_ref/vcam_mockup.png` (approved screenshot).
- `doc/director_kit/mockup_src/VCamPage.dc.html` (script section: `cardsFor`, `rowView`, `tileView`, `renderVals`, `bindDropped`, `reassignFace`, `addCam`, `removeCam`, `addFace`).
- `scripts/code_tools/director_kit_gen.py` (`data_kind`, `control_for`, `page_attrs`, `kit_page`, `classify`, `combo` item emission).
- `scripts/code_tools/director_kit/director_mockup_data.json` (existing `camera_group/vcam_tab` entry and row syntax from other pages), `scripts/code_tools/director_kit/kit_pages.json` (p0_pages config), `doc/director_kit/page_sources.json`, `scripts/code_tools/director_kit/gen_report.txt`.
- `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml` (generated output example), `indra/newview/skins/default/xui/en/panel_dpage_camera_vcam.xml` (current output).
- `indra/newview/app_settings/settings.xml` (Prism setting definitions).

Status codes used in section B:
- **D**: the row and its binding are both named in the spec.
- **P**: the row is in the spec, but a member, field, text format, or option mapping is not named.
- **UNDEFINED IN SPEC**: the spec does not define the row, or does not define its binding. Copy-only notes are marked `(copy)`.

Widget vocabulary: `switch` (toggle), `combo`, `slider`, `button`, `segmented` (radio-style, two to four options), `readout` (text value), `note` (copy-only text), `custom` (no generator kind).

Row counting rule: one row = one row spec with a stable ID, across all variants (object/virtual/skeleton/lens camera types, Fit/Fill, prim/virtual faces, guide on/off, FOV mode, target/automatic rate). Dynamic instances (8 tiles, up to 8 chips, face list entries) are counted once under their row.

---

## A. Page hierarchy, top to bottom

The mockup order is used. Spec section 6.1 says the first full-span card holds the monitor strip; the master card is shown above it in the mockup and in spec section 1. See E.1.

```
Kit chrome (not VCam rows): preset combo "Custom", "Save as...", Undo/Redo, "Find a control...", right rail "About this page"
Page master: "Virtual Cam" switch (PrismLensEnabled)
  Card "Virtual Cam" (master card, full width)
    readout  summary line          (MST-SUM)
    readout  perf / status line    (MST-PERF)
    button   Open full manager     (MST-MGR)
Card "Monitors" (custom, full width)
  custom   monitor strip           (MON-STRIP)
    segmented  Large | Compact     (MON-LC)
    custom     page pager 1-4/5-8  (MON-PAGE, Large mode with more than 4 cameras)
    switch     Previews            (MON-PREV)
    segmented  Speed 0.5/1/2/5 fps (MON-SPEED)
    custom     tiles x8            (TILE, one row, eight slots)
    readout    footer summary      (MON-FOOT)
Camera selector (outside cards, custom)
  custom   "Settings for" chips Cam 1..Cam 8  (SEL-CHIPS)
  combo    "+ Add camera..."       (SEL-ADD, native-capable, code-bound)
  button   "Remove capture"        (SEL-REMOVE, native-capable, code-bound)
Signal chain (kit sections = chips; each step has an intro line)
  Step 1  Source
    Card "Source" (variant: object | virtual | skeleton | lens)
    Card "Placement" (object or virtual variant)
    Card "Skeleton POV" (skeleton variant only)
  Step 2  Lens
    Card "Optics"
    Card "Surface lens" (lens variant only; global)
    Card "In-world guide"
  Step 3  Look
    Card "Camera look"
    Card "Where this look goes"
  Step 4  Output
    Card "Output frame"
    Card "Faces this camera feeds" (custom list)
    Card "Face mapping · <face>" (only when a face is selected)
    Card "TV effects · <face>"   (only when a face is selected)
    Card "Showing the raw camera output" (only when no face is selected)
  Step 5  Rate & performance
    Card "Picture rate"
    Card "Performance" (global)
```

Step chips: `1 Source`, `2 Lens`, `3 Look`, `4 Output`, `5 Rate & performance`. Chips are kit sections, not rows (`kit_page.section`).

Totals: 126 rows (section B), 5 custom rows, 27 UNDEFINED IN SPEC rows (12 non-copy, 15 copy-only notes).

---

## B. Row inventory

### B.1 Master card and monitor strip

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| MST-SW | Page master | switch | Virtual Cam | `PrismLensEnabled` (setting, global; spec 3.1) | Global (All cameras) | D |
| MST-SUM | Master card | readout | summary line, `VCam N/8 cameras · N/16 faces (V visible)` | Registry counts from `RegistrySnapshot` (spec 3.3, 6.2 configurationChanged). Text format not in spec. | Global | P |
| MST-PERF | Master card | readout | perf line (Presented FPS, render p95, scale, admitted attempts/s, Adaptive state) | Performance snapshot (spec 6.2 performanceChanged). Field list and text not in spec. | Global | P |
| MST-MGR | Master card | button | Open full manager | Opens existing Virtual Cam Manager (spec 1 only; no 6.3 row) | Global | P |
| MON-STRIP | Monitors card | custom (monitor_strip) | Monitors (card title) | `ALVCamMonitorStrip` over `LLPrismPreview` tile state (spec 4.1, 4.4) | Global with per-capture tiles | D (custom) |
| MON-LC | Strip header | segmented | Large / Compact | UI session layout mode. Spec 4.4 names Large (pages 1-4 / 5-8, as in the mockup) and geometry only; no toggle or persistence named. | Page (UI session) | P |
| MON-PAGE | Strip header (Large, more than 4 cameras) | custom pager | `1–4 / 5–8` with prev/next | UI session page index (spec 4.4 pages 1-4 / 5-8) | Page | D (custom) |
| MON-PREV | Strip header | switch | Previews | `PrismMonitorPreviewsEnabled` (NEW setting, spec 3.2 and 4.2; not in settings.xml yet, added by VC1) | Global (All cameras) | D |
| MON-SPEED | Strip header | segmented (4) | Speed: 0.5 fps, 1 fps, 2 fps, 5 fps | `PrismMonitorPreviewFPS` (NEW setting, spec 3.2; accepted .5, 1, 2, 5) | Global (All cameras) | D |
| TILE | Monitors (8 slots) | custom (tile) | per-tile fields, section D.2 | Tile state from `LLPrismPreview::tileState(capture)` and atlas UV (spec 4.1); selection `mSelectedCapture` (3.2); display choice `mMonitorDisplay[capture]` (3.2); drag `DAD_VCAM_FEED` (5.1) | Per capture (Cam N) | P (see E.8) |
| MON-FOOT | Monitors footer | readout | footer summary (D.2) | Derived from preview budget (spec 4.2 entitlement fields). No footer row in spec. | Global | UNDEFINED IN SPEC |

### B.2 Camera selector (outside cards)

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| SEL-CHIPS | Selector row | custom (chip radio group, up to 8) | "Settings for": Cam 1 ... Cam 8, dot = live / preview / idle | `mSelectedCapture` (UI session, spec 3.2). Separate from monitors (spec 1, 6.1). | Per capture (Cam N) | D (custom) |
| SEL-ADD | Selector row | combo (action menu) | "+ Add camera..." (placeholder); at cap: "8 of 8" | `addFromSelectedObject`, `addVirtualAtView`, `addOtsPair`, `addSurfaceLens` (spec 3.3, 6.4). OTS disabled when one slot is left (6.4). Code-bound combo. | Global (8 capture cap) | D |
| SEL-REMOVE | Selector row | button | Remove capture | `removeCapture()`, current confirmation semantics (spec 6.4) | Per capture (Cam N) | D |

### B.3 Step 1 Source

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| SRC-TYPE | Source (object / virtual / skeleton) | segmented (3) | Source: Object, Virtual, Skeleton POV | No type-conversion API in spec. Spec 6.3 says Surface Lens is "not convertible in place" and says nothing about other types. | Per capture (Cam N) | UNDEFINED IN SPEC |
| SRC-BOUND | Source | readout | Bound to (object) / Stored at (virtual) / Driven by (skeleton) | `mCameraObjectId` (object, spec 6.3 Source); `CameraSettings::mVirtualPos` (virtual); `CameraSettings::mBonePov` anchor and joint (skeleton) | Per capture | D |
| SRC-NOTE-OBJ | Source (object) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| SRC-NOTE-VIRT | Source (virtual) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| SRC-NOTE-SKEL | Source (skeleton) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| SRC-LENS-TYPE | Source (lens) | readout | Type: Surface lens | Fixed capture type identity (spec 6.3 Surface Lens) | Per capture | D |
| SRC-LENS-APER | Source (lens) | readout | Aperture | Required aperture display (spec 6.3; 7.3) | Per capture | D |
| SRC-LENS-LOCATE | Source (lens) | button | Locate aperture | Locate selects and flashes the required display (spec 6.3) | Per capture | D |
| SRC-LENS-NOTE | Source (lens) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |

### B.4 Placement (object and virtual variants)

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| PL-SET | Placement (object) | button | Set selected camera | `setSelectedCamera()` (`llprismlens.h:617-621`, spec 6.3) | Per capture | D |
| PL-FRONT | Placement (object) | button | Place eye in front | No API in spec | Per capture | UNDEFINED IN SPEC |
| PL-NOTE | Placement (object) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| PL-EX | Placement adv (object) | slider | Eye offset X (m), -64..64, step 0.01, 3 dp | `CameraSettings::mLocalEyeOffset` (vector, spec 6.3). Per-axis split not in spec. | Per capture | P |
| PL-EY | Placement adv (object) | slider | Eye offset Y (m), same range | same | Per capture | P |
| PL-EZ | Placement adv (object) | slider | Eye offset Z (m), same range | same | Per capture | P |
| PL-SNAP | Placement (virtual) | button | Snap to my view | Factored current-view transform, `CameraSettings::mVirtual`, `mVirtualPos`, `mVirtualRot` (spec 6.3; `llfloaterprismmanager.cpp:1907-1983`) | Per capture | D |
| PL-NOTE-V | Placement (virtual) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |

### B.5 Skeleton POV (skeleton variant only)

Spec 6.3 names `CameraSettings::mBonePov` and `ALVCamBonePov::status()` (`alvcambonepov.h:20-27`) but no sub-field names. Every sub-field below is P.

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| BP-ANCHOR | Skeleton POV | combo | Anchor subject: Me, Subject A, Subject B, Subject C, Subject D | `mBonePov` anchor (field name not in spec) | Per capture | P |
| BP-JOINT | Skeleton POV | combo (10) | POV joint: Eyeline, mHead, mNeck, mChest, mPelvis, mShoulderRight, mElbowRight, mWristRight, mWristLeft, mFootRight | `mBonePov` joint (field not in spec). Off-spine joint forces Aim = Stabilized (mockup behaviour, not in spec). | Per capture | P |
| BP-AIM | Skeleton POV | combo | Aim mode: Full-follow, Stabilized | `mBonePov` aim (field not in spec) | Per capture | P |
| BP-ROLL | Skeleton POV | combo | Roll mode: Horizon-lock, Inherit roll | `mBonePov` roll (field not in spec) | Per capture | P |
| BP-SMOOTH | Skeleton POV | slider | Smooth (s), 0..10, step 0.05, 2 dp | `mBonePov` smoothing (field not in spec) | Per capture | P |
| BP-STATUS | Skeleton POV | readout | Status: `Attached: <anchor>, <joint>[ (off-spine)]` | `ALVCamBonePov::status()` (spec 2, `alvcambonepov.h:20-27`) | Per capture | D |
| BP-OX | Skeleton POV adv | slider | Offset X (m), -64..64, step 0.01, 3 dp | `mBonePov` offset (field not in spec) | Per capture | P |
| BP-OY | Skeleton POV adv | slider | Offset Y (m), same | same | Per capture | P |
| BP-OZ | Skeleton POV adv | slider | Offset Z (m), same | same | Per capture | P |
| BP-TPITCH | Skeleton POV adv | slider | Trim pitch (°), -180..180, step 0.5, 1 dp | `mBonePov` trim (field not in spec) | Per capture | P |
| BP-TYAW | Skeleton POV adv | slider | Trim yaw (°), -180..180, step 0.5, 1 dp | same | Per capture | P |
| BP-SCALE | Skeleton POV adv | switch | Clone-scale aware | `mBonePov` (field not in spec) | Per capture | P |
| BP-RESET | Skeleton POV adv | button | Reset attach | Reset of typed settings (spec 5.4 mentions preset/reset records; this button's scope not named) | Per capture | P |

### B.6 Step 2 Lens

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| LN-EQ | Optics | readout | Lens equiv.: `<mm> mm full frame · <deg>° V` | Derived from vertical FOV on a 24 mm sensor height (mockup tooltip; not in spec) | Per capture | UNDEFINED IN SPEC |
| LN-FOVMODE | Optics (not skeleton) | combo | Field of view: Fixed vertical FOV, Follow projector | `CameraSettings` FOV mode (spec 6.3 Lens) | Per capture | D |
| LN-DEG | Optics (fixed FOV) | slider | Degrees (°), 5..175, step 0.5, 1 dp | `CameraSettings` fixed vertical FOV (spec 6.3) | Per capture | D |
| LN-FROMPROJ | Optics (follow projector) | readout | Degrees: `From projector (60.0°)` | Projector-supplied FOV readout. Spec names the mode only. | Per capture | UNDEFINED IN SPEC |
| LN-FOVATT | Optics (skeleton) | slider | FOV (attached) (°), 10..150, step 1, 0 dp | `BonePovSettings::mFovDeg` (spec 6.3 Lens) | Per capture | D |
| LN-NEAR | Optics | slider | Near clip (m), 0.01..10, step 0.01, 3 dp | `CameraSettings` near clip (spec 6.3 "near/far clip") | Per capture | D |
| LN-FAR | Optics | slider | Far clip (m), 0.2..512, step 0.1, 1 dp | `CameraSettings` far clip (spec 6.3) | Per capture | D |
| LN-ZOOM | Surface lens (lens variant only) | slider | Lens zoom (×), 1..8, step 0.1, 2 dp | `PrismLensZoom` (global setting, spec 6.3 Lens; badge All cameras) | Global (All cameras) | D |
| LN-ZOOM-NOTE | Surface lens | note | copy (D.4) | none | Global | UNDEFINED IN SPEC (copy) |
| GD-SHOW | In-world guide | switch | Show guide | `CameraSettings::mShowGuide` (spec 6.3 Lens) | Per capture | D |
| GD-THIRDS | In-world guide (guide on) | switch | Rule of thirds | "guide flags" (spec 6.3). Member name not in spec. | Per capture | P |
| GD-UPROLL | In-world guide (guide on) | switch | Up / roll | same | Per capture | P |
| GD-CROSS | In-world guide (guide on) | switch | Center crosshair | same | Per capture | P |
| GD-CLIP | In-world guide (guide on) | switch | Clip range markers | same | Per capture | P |
| GD-NOTE | In-world guide (guide off) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |

### B.7 Step 3 Look

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| LK-EXP | Camera look | slider | Exposure EV (EV), -4..4, step 0.1, 1 dp | `CameraSettings::mOptics` exposure (spec 6.3 Look) | Per capture | D |
| LK-GRAIN | Camera look | slider | Film grain, 0..1, step 0.01, 2 dp | `CameraSettings::mOptics` (spec 6.3) | Per capture | D |
| LK-CA | Camera look | slider | Chromatic ab., 0..1, step 0.01, 2 dp | `CameraSettings::mOptics` (spec 6.3) | Per capture | D |
| LK-CRT | Camera look | slider | CRT lines, 0..1, step 0.01, 2 dp | `CameraSettings::mOptics` capture CRT lines (spec 6.3) | Per capture | D |
| LK-FEEDS | Where this look goes | readout | Feeds: face names joined, or "No faces yet" | Derived from the face list (spec 6.3 Output). No row in spec. | Per capture | UNDEFINED IN SPEC |
| LK-NOTE | Where this look goes | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| LK-GOOUT | Where this look goes | button | Go to Output | UI navigation to step 4. No model binding in spec. | Page | UNDEFINED IN SPEC |

### B.8 Step 4 Output

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| OF-ASPECT | Output frame | combo | Output aspect: 16:9, 4:3, 1:1, Custom | `CameraSettings::mOutputAspect` (spec 6.3 Output) | Per capture | D |
| OF-CUSTOM | Output frame (aspect Custom) | slider | Custom W/H, 0.25..4, step 0.01, 3 dp | Storage for custom ratio not in spec (spec names mOutputAspect only) | Per capture | UNDEFINED IN SPEC |
| OF-PIC | Output frame | readout | Picture: `<w> × <h> px (scale <q>)` | Derived from aspect and Quality ceiling. Spec 4.3 gives the logical-size rule but no row. | Per capture | UNDEFINED IN SPEC |
| FACE-LIST | Faces this camera feeds | custom (radio rows with drop targets) | Raw row + one row per face (name, meta, visibility) | `RegistrySnapshot::mDisplays` filtered by capture (spec 6.3 Output); selection `mMonitorDisplay[capture]` (3.2); drop calls `reassignDisplay` (5.2) | Per capture (rows = per display) | D (custom) |
| FACE-ADD | Faces this camera feeds | button | Add selected face | `bindSelectedFace` / `addSelectedDisplay` path (spec 5.2, 6.3) | Per capture | D |
| FACE-NEWFIT | Faces this camera feeds | combo | Mapping for the new face: Fit, Fill, Stretch | Page default for new faces (spec 5.2 "new face mapping" default). Storage not defined. | Page (UI session) | P |
| FACE-SCREEN | Faces this camera feeds | button | Add virtual screen | "existing engine calls" (spec 6.3); call not named | Per capture | P |
| FACE-COUNT | Faces this camera feeds | readout | `N of 16 faces used (all cameras)` | Registry display count; 16 cap (spec 5.2, 6.4). Format not in spec. | Global | P |
| MP-FEED | Face mapping (per face) | segmented (2) | Feed from: This camera, Gate program | `setDisplayGateSubscribed()` (spec 6.3 Output) | Per display (This face) | D |
| MP-FIT | Face mapping | combo | Mapping: Fit, Fill, Stretch | `DisplaySettings` Fit/Fill/Stretch via `setDisplaySettings()` (spec 3.1, 6.3) | Per display | D |
| MP-ANCX | Face mapping (Fill only) | slider | Anchor X, 0..1, step 0.05, 2 dp | `DisplaySettings` anchor (spec 3.1, 6.3) | Per display | D |
| MP-ANCY | Face mapping (Fill only) | slider | Anchor Y, 0..1, step 0.05, 2 dp | `DisplaySettings` anchor (spec 3.1, 6.3) | Per display | D |
| MP-SHAPE | Face mapping | readout | Face shape: `<aspect> virtual screen|prim face · visible|offscreen` (warn when not visible) | Derived from display visibility. Not in spec. | Per display | UNDEFINED IN SPEC |
| MP-HEIGHT | Face mapping (virtual only) | slider | Height (m), 0.1..20, step 0.05, 2 dp | Virtual screen height, `DisplaySettings::mHeight` path (spec 6.3: mVirtual/mPos/mRot/mWidth/mHeight) | Per display | D |
| MP-SCAR | Face mapping (virtual only) | combo | Screen aspect: 16:9, 4:3, 1:1, Custom | mWidth/mHeight path (spec 6.3). Which field carries the ratio is not named. | Per display | P |
| MP-LOCATE | Face mapping | button | Locate | Flash this display in-world via page model (spec 6.3 "existing engine calls"; call not named) | Per display | P |
| MP-REPOS | Face mapping (virtual only) | button | Reposition to my view | mVirtual/mPos/mRot path (spec 6.3). "A few metres in front" policy not in spec. | Per display | P |
| MP-REMOVE | Face mapping | button | Remove face | `unbindDisplay` (named as the inverse of bind in spec 5.4) | Per display | D |
| MP-GATE-NOTE | Face mapping (Gate feed) | note | copy (D.4) | none | Per display | UNDEFINED IN SPEC (copy) |
| MP-BARR | Face mapping adv | slider | Bar R, 0..1, step 0.05, 2 dp | `DisplaySettings` linear bar colour (spec 2, 3.1) | Per display | D |
| MP-BARG | Face mapping adv | slider | Bar G, 0..1, step 0.05, 2 dp | same | Per display | D |
| MP-BARB | Face mapping adv | slider | Bar B, 0..1, step 0.05, 2 dp | same | Per display | D |

### B.9 TV effects (per face)

Every TV control is bound to `DisplaySettings::mEffects` (spec 6.3 "Presets and every TV control"); spec 3 names `ScreenEffects` as the struct. Individual effect field names are not in spec, so the basic and advanced controls are D at struct level.

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| TV-PRESETS | TV effects | button row (5) | Clean, Old CRT, Broken TV, VHS Tape, Security Feed | `DisplaySettings::mEffects` via shared preset helpers (spec 6.3; values in `llfloaterprismmanager.cpp:279-319`). Names found in `floater_prism_manager.xml`. | Per display | P |
| TV-SCAN | TV effects | slider | Scanlines, 0..1, step 0.05, 2 dp | `DisplaySettings::mEffects` (spec 6.3) | Per display | D |
| TV-GRAY | TV effects | slider | Grayscale, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-STATIC | TV effects | slider | Static Noise, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-VIG | TV effects | slider | Vignette, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-BRIGHT | TV effects | slider | Brightness, -1..1, step 0.05, 2 dp | same (separate from camera Exposure EV) | Per display | D |
| TV-FLIPH | TV effects | switch | Flip H | Flip in `DisplaySettings` / `ScreenEffects` (spec 3) | Per display | D |
| TV-FLIPV | TV effects | switch | Flip V | same | Per display | D |
| TV-ROT90 | TV effects | switch | Rotate 90 | Rotation in `DisplaySettings` / `ScreenEffects` (spec 3) | Per display | D |
| TV-LINES | TV effects adv | slider | Lines, 20..1200, step 1, 0 dp | `DisplaySettings::mEffects` (spec 6.3) | Per display | D |
| TV-DARK | TV effects adv | slider | Darkness (%), 0..100, step 1, 0 dp | same | Per display | D |
| TV-SHARP | TV effects adv | slider | Sharpness (%), 0..100, step 1, 0 dp | same | Per display | D |
| TV-THICK | TV effects adv | slider | Line thickness (%), 10..90, step 1, 0 dp | same | Per display | D |
| TV-PIXEL | TV effects adv | slider | Pixelate, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-SEPIA | TV effects adv | slider | Sepia Tint, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-CHROMA | TV effects adv | slider | Chroma Bleed, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-VROLL | TV effects adv | slider | Vertical Roll, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-ROLLSPD | TV effects adv | slider | Roll Speed, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-TRACK | TV effects adv | slider | VHS Tracking, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-FLICK | TV effects adv | slider | Flicker, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-INTER | TV effects adv | slider | Interlace, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-DROP | TV effects adv | slider | Dropout, 0..1, step 0.05, 2 dp | same | Per display | D |
| TV-SHEEN | TV effects adv | slider | Sheen, 0..1, step 0.05, 2 dp | same | Per display | D |
| RAW-NOTE | Showing the raw camera output (no face) | note | copy (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |

### B.10 Step 5 Rate and performance

| ID | Where | Widget | Label (units) | Binding (setting or API) | Scope | Status |
|---|---|---|---|---|---|---|
| RT-MODE | Picture rate | combo | Mode: Automatic, Target FPS | `CaptureRateSettings` picture mode via `setCaptureRateSettings()` (spec 6.3 Rate) | Per capture | D |
| RT-FPS | Picture rate (Target FPS only) | slider | Target FPS (fps), 1..30, step 1, 0 dp | `CaptureRateSettings` target FPS (spec 6.3) | Per capture | D |
| RT-PRESET | Picture rate (Target FPS only) | combo | Preset: Pick…, 5, 10, 12, 15, 20, 23.976, 24, 25, 29.97, 30 | Not in spec. Would write target FPS and mode through `setCaptureRateSettings()`. Non-integer values (23.976, 29.97) cannot be set on the integer target slider. | Per capture | UNDEFINED IN SPEC |
| RT-READOUT | Picture rate | readout | Req/Ent/Obs: `<req>/<ent>/<obs> fps` | Capture runtime snapshot (spec 6.3 Rate: Requested / Entitled / Observed / output age). Mockup omits output age. | Per capture | D |
| RT-NOTE | Picture rate | note | copy, three variants (D.4) | none | Per capture | UNDEFINED IN SPEC (copy) |
| PF-PROTECT | Performance | switch | Protect main view | `PrismAdaptivePerformance` (setting, global; settings.xml) | Global (All cameras) | D |
| PF-MAINFPS | Performance | combo | Main-view target: 30 FPS, 45 FPS, 60 FPS | `PrismProtectedMainFPS` (setting, global; valid 30/45/60) | Global (All cameras) | D |
| PF-BUDGET | Performance | combo | Capture budget: Adaptive 30 / Manual every frame, 5, 10, 15, 20, 30 attempts/s | `PrismCaptureRefreshCeilingHz` (setting, global; valid 5/10/15/20/30, 0 = Every Frame in Manual). "Adaptive 30" has no defined value. | Global (All cameras) | P |
| PF-QUAL | Performance | slider | Quality ceiling (×), 0.25..2, step 0.25, 2 dp | `PrismLensResolutionScale` (setting, global; clamp 0.25..2.0) | Global (All cameras) | D |
| PF-ADMIT | Performance | readout | Admitted: `<x> of <budget> attempts/s` | Sum of entitled attempts vs budget. Row not in spec. | Global | UNDEFINED IN SPEC |
| PF-PREV | Performance | readout | Previews: `Off (no cost)` / `On · <fps> fps · <n> attempts/s` / `On · <fps> fps · free` | Mirror of effective preview cost (spec 6.3: performance card mirrors the readout; no second control) | Global | P |
| PF-NOTE-1 | Performance adv | note | copy (D.4) | none | Global | UNDEFINED IN SPEC (copy) |
| PF-NOTE-2 | Performance adv | note | copy (D.4) | none | Global | UNDEFINED IN SPEC (copy) |
| PF-NOTE-3 | Performance adv | note | copy (D.4) | none | Global | UNDEFINED IN SPEC (copy) |

Per-status totals (verify with grep): D and P counts are in section F.

---

## C. Draft generator entries (plain setting-bound rows only)

Scope of drafts: rows with a settings.xml key or a new setting from spec 3.2. Per-capture and per-display rows (API-bound, code-bound) have no draft and are deferred to `ALVCamKitAdapter`. Custom rows (monitor strip, tiles, pager, selector chips, face list) have no draft.

Syntax follows `director_mockup_data.json` rows: `l` label, `k` kind, `w` widget, `h` help, `kw` keywords (proposed), `s` setting, `d` default, `mn`/`mx`/`st`/`dc` range, `unit`, `it` items, `ty` type. The page master uses the `master` object, as `polish_tab` does.

### C.1 Page-level entries

```json
{
  "name": "vcam_tab",
  "label": "VCam",
  "sum": "Extra virtual cameras fed from captures or camera feeds, managed in Virtual Cam Manager.",
  "pre": "none",
  "master": {"w": "vc_enable", "l": "Virtual Cam", "s": "PrismLensEnabled"},
  "cards": [ "...see section A for card order; IDs below..." ],
  "sections": [
    {"id": "source", "label": "Source", "cards": ["..."]},
    {"id": "lens",   "label": "Lens",   "cards": ["..."]},
    {"id": "look",   "label": "Look",   "cards": ["..."]},
    {"id": "output", "label": "Output", "cards": ["..."]},
    {"id": "rate",   "label": "Rate & performance", "cards": ["..."]}
  ]
}
```

The existing entry in `director_mockup_data.json` (`camera_group/vcam_tab`, one card `lens_vcam_card`, 3 rows, `"component": "VCamPage"`) is replaced. `component` is not read by `director_kit_gen.py`.

`kit_pages.json` entry (`p0_pages`):

```json
"camera_group/vcam_tab": {
  "v1_sources": [],
  "root_class": "panel_director_vcam",
  "root_name": "vcam_tab_logic",
  "undo_context": "vcam",
  "label_names": {}
}
```

Why it is needed: `classify()` checks `p0_pages` first. Without this entry, `camera_group/vcam_tab` stays `legacy` (`gen_report.txt:34`, `hosted_v1_nodes=1`). `undo_context` value "vcam" is proposed; spec does not name it.

### C.2 Setting-bound row drafts

**C.2.1 Page master (MST-SW), `master` object above.** Help: `Master render toggle (PrismLensEnabled). Captures and faces stay editable while it is off.`

**C.2.2 MON-PREV (Previews switch, inside the monitor strip). Reference draft; the strip itself is custom.**

```json
{"l": "Previews", "k": "toggle", "w": "vc_previews",
 "h": "One switch for every monitor. Off stops all preview rendering, which saves FPS. Faces in-world keep updating.",
 "kw": "preview monitor render fps", "s": "PrismMonitorPreviewsEnabled", "d": 1, "ty": "check"}
```

**C.2.3 MON-SPEED (Speed, inside the monitor strip). BLOCKED.**

```json
{"l": "Speed", "k": "combo", "w": "vc_preview_speed",
 "h": "Preview refresh rate shared by all monitors",
 "kw": "preview fps speed", "s": "PrismMonitorPreviewFPS", "d": 1.0,
 "it": ["0.5 fps", "1 fps", "2 fps", "5 fps"], "ty": "radio"}
```
BLOCKED: `combo` items get `value = index` (`director_kit_gen.py:464-465`), so 0.5/1/2/5 cannot be stored. The generator has no segmented kind (see E.2).

**C.2.4 LN-ZOOM (Lens zoom; card "Surface lens", only when the camera is a lens).**

```json
{"l": "Lens zoom", "k": "slider", "w": "vc_lens_zoom",
 "h": "Magnification shared by every Surface Lens (PrismLensZoom).",
 "kw": "surface lens zoom magnify", "s": "PrismLensZoom", "d": 2.0,
 "mn": 1, "mx": 8, "st": 0.1, "dc": 2, "unit": "×", "ty": "slider"}
```
Scope badge "All cameras": no data key exists (E.5).

**C.2.5 PF-PROTECT (Protect main view).**

```json
{"l": "Protect main view", "k": "toggle", "w": "vc_protect",
 "h": "Reduce Prism scale or cadence, or suspend captures, to protect the main-view target.",
 "kw": "adaptive protect main view performance", "s": "PrismAdaptivePerformance", "d": 1, "ty": "check"}
```

**C.2.6 PF-MAINFPS (Main-view target). BLOCKED.**

```json
{"l": "Main-view target", "k": "combo", "w": "vc_main_fps",
 "h": "Presented main-view FPS that Adaptive protects.",
 "kw": "main view target fps protect", "s": "PrismProtectedMainFPS", "d": 30,
 "it": ["30 FPS", "45 FPS", "60 FPS"], "ty": "combo"}
```
BLOCKED: stored values are 30/45/60; the generator writes index values 0/1/2 (E.4).

**C.2.7 PF-BUDGET (Capture budget). BLOCKED.**

```json
{"l": "Capture budget", "k": "combo", "w": "vc_capture_budget",
 "h": "Total capture budget: attempts per second shared by all cameras. Not a camera’s picture FPS.",
 "kw": "budget attempts capture rate", "s": "PrismCaptureRefreshCeilingHz", "d": 30,
 "it": ["Adaptive 30 / Manual every frame", "5 attempts/s", "10 attempts/s", "15 attempts/s", "20 attempts/s", "30 attempts/s"],
 "ty": "combo"}
```
BLOCKED: stored values are 0/5/10/15/20/30 with 0 = Every Frame in Manual mode. The mockup's first label is ambiguous between 0 and 30.

**C.2.8 PF-QUAL (Quality ceiling).**

```json
{"l": "Quality ceiling", "k": "slider", "w": "vc_quality_ceiling",
 "h": "Resolution scale ceiling for every capture.",
 "kw": "quality resolution scale ceiling", "s": "PrismLensResolutionScale", "d": 1.0,
 "mn": 0.25, "mx": 2, "st": 0.25, "dc": 2, "unit": "×", "ty": "slider"}
```

### C.3 Custom rows (no draft; generator cannot emit them)

| ID | Custom row | Why no draft |
|---|---|---|
| MON-STRIP | monitor strip (header, tiles, footer) | Generator rejects `k: custom` with a non-native `ty` (`director_kit_gen.py:395-398, 442-443`). Needs a new kind. |
| TILE | 8 tile slots | Same. |
| MON-PAGE | pager | Same. |
| SEL-CHIPS | camera chips | Same. |
| FACE-LIST | face radio list with drop targets | Same. |

Code-bound rows (no `s`, `bind: "code"`, per the rig page example): SEL-ADD, SEL-REMOVE, and all per-capture and per-display rows. These are deferred to the ALVCamKitAdapter draft.

---

## D. Text strings (exactly as in the mockup)

Placeholders: `N` camera number, `Name` camera name, `<face>` face name, `<fit>` Fit/Fill/Stretch, `<preset>` preset name, `<x>` a value.

### D.1 Static page text

- Page title: `VCam page`; breadcrumb in the mockup chrome: `Director › Camera › VCam` (kit chrome).
- Master: `Virtual Cam`. Master help: `Master render toggle (PrismLensEnabled). Captures and faces stay editable while it is off.`
- MST-SUM: `VCam <N>/8 cameras · <N>/16 faces (<V> visible)` (mockup sample: `VCam 4/8 cameras · 4/16 faces (3 visible)`).
- MST-PERF (on): `Presented 60 FPS · render p95 4.2 ms · scale <q> · admitted <a> attempts/s · Adaptive: steady, protecting <fps>` (sample uses 1.00 and 50.0 and 30 FPS). With Adaptive off: `Adaptive off`.
- MST-PERF (master off): `Virtual Cam is off. Nothing renders; cameras and faces stay editable.`
- MST-MGR: `Open full manager`. Status on click: `Opening the full Virtual Cam Manager (Captures, Displays, Performance, Gate).`
- Monitors title: `Monitors`. Title help: `What each camera sends out. Drag a monitor onto a surface to show that camera there.`
- Large / Compact: `Large` and `Compact`. Help, Large: `Four big monitors with status; more than four scroll sideways.` Help, Compact: `Up to eight small monitors in one row; status shown as a dot.` Automatic suffix: ` (Automatic picks Compact for N cameras.)` or `Large`.
- Pager: `Show cameras 1 to 4`, `Show cameras 5 to <max>`; label `1–4 / 5` or `1–4 / 5–8`.
- Previews: `Previews`. Help: `One switch for every monitor. Off stops all preview rendering, which saves FPS. Faces in-world keep updating.`
- Speed: `Speed`. Option help: `Preview refresh rate shared by all monitors`. Options: `0.5 fps`, `1 fps`, `2 fps`, `5 fps`.
- Monitor footer, preview on, unshown cameras present: `<N> monitors at <R> fps. <F> reuse their camera’s last picture (free); <K> (Cam 3, Cam 4) render only for the preview, about <X> extra attempts/s from the shared budget. Drag a monitor onto a surface to show it there.` (singular `renders` for K=1)
- Footer, preview on, all cameras have faces: `<N> monitors at <R> fps, all reusing their camera’s last picture: no extra renders. Drag a monitor onto a surface to show it there.`
- Footer, previews off: `Previews are off: 0 of <N> monitors render. Faces in-world keep updating at their own picture rate.`
- Footer, Virtual Cam off: `Virtual Cam is off, so monitors and faces are paused.`
- Selector: label `Settings for`. Chip help: `Edit Cam <N> · <Name>`. Add label (hidden): `Add a camera`. Add placeholder: `+ Add camera…`; at cap: `8 of 8`. Options: `From selected object`, `Virtual cam at my view`, `OTS pair from Subjects A/B`, `Surface lens from selected face`. Remove button: `Remove capture`.
- Step tab group aria: `Camera settings, in signal order`. Chips: `1 Source`, `2 Lens`, `3 Look`, `4 Output`, `5 Rate & performance`.
- Card scope tags: `Cam <N>`, `This face`, `All cameras`. Scope help: `Global: changes here affect every camera.` / `Applies to the selected face only.` / `Applies to this camera only.`
- Reset labels: `Reset <card title>` and `Reset <row label>`. Advanced toggle: `More options (<n>)` / `Fewer options`.
- Status line default: `Monitors are previews only. Pick the camera to edit with “Settings for”.`
- Drop bar region aria: `Send to surface`. Drop bar text: `Drop in-world on any prim face to show Cam <N> there`. Drop bar sub (dragging): `In the viewer you drop on the face itself. Here, drop on this bar to simulate it, or on a face row in 4 · Output to move that face.` Drop bar sub (send mode): `Keyboard: select a prim face in-world, then bind it. Or move an existing face to this camera.`
- Send mode buttons: `Use selected in-world face`, `Cancel`. Reassign select: placeholder `Move an existing face here…`; option `<face> (now Cam <N>)`.
- Toast texts: `Cam <N> → new face`, `Cam <N> → <face>`.
- Right rail (kit-owned): `About this page`; the page summary is the `sum` text above. Stats in the mockup (`Cards 1`, `Sections 1`, `Controls 3`, `In More options 0`, `Presets saved only`) are stale (E.11). Footer hint: `Home resets the focused control. Right-click a cast member for its menu. ? in the transport lists every hotkey.` (kit-owned).

### D.2 Tile text

- Figure drag help: `Drag onto a surface to show Cam <N> there`.
- Figure aria: `Monitor for Cam <N>, <Name>. <feed>. <state>.`
- Badge (on): `● PREVIEW <R> fps`; badge (paused): `❚❚ PAUSED`.
- Tag: `EDITING` (on the selected tile, large mode).
- Time overlay: 12-hour clock, e.g. `9:23:31 PM`.
- Name: large `Cam <N> · <Name>`; compact `Cam <N>`. Name tooltip: `Cam <N> · <Name>`.
- Feed line: `Shows: camera output (raw)`, or `Shows: <face> · <fit>[ · <preset>][ · Gate]`.
- State line (live): `Live <obs>/<req> fps · <n> face(s)` (`face` singular for one face, `faces` otherwise). State (preview only): `Preview only · no visible face`. State (idle): `Idle · no visible face`. State (paused): `Paused · Virtual Cam off`.
- Off overlay title: `Preview off` (Virtual Cam on) or `Virtual Cam off` (master off). Subtitle: `saving FPS` or `nothing renders`.
- Menu button aria: `More for Cam <N>`. Menu items: `Edit settings`, `Send to surface…`, `Show raw output` (only when a face is bound).
- Mockup sample tile lines: `Live 24/24 fps · 2 faces`, `Preview only · no visible face`.

### D.3 Face rows and camera selector

- Face list aria: `Choose what the monitor shows and which face to edit`.
- Raw row: name `Camera output (raw)`; meta `<aspect> · before any face mapping`; visibility `Monitor only`; help `Show the unmapped camera picture on the monitor.`
- Face row: meta `Prim face` or `Virtual screen`, then ` · <aspect> · <fit>` plus ` · <preset>` and ` · Gate` as applicable; visibility `Visible` or `Offscreen`; help `Show this face on the monitor and edit it below. Drop another camera’s monitor here to move the face to that camera.`
- Drop-target meta (while dragging another camera): `Drop to show Cam <N> on this face instead`.
- Add selected face help: `Bind the face you have selected in-world to this camera. No extra scene render.`
- Add virtual screen help: `Create a prim-free virtual screen for this feed at your current view. Needs no rezzed object; works in no-rez parcels.`
- New-face mapping label (hidden): `Mapping for the new face`.
- Face count: `<N> of 16 faces used (all cameras)`.
- Chip help: `Edit Cam <N> · <Name>`.

### D.4 Row labels, help, and copy (by row ID)

Source
- SRC-TYPE: Source. Help: `Object: follows an in-world prim. Virtual: a stored transform, no prim. Skeleton POV: a virtual camera driven by a cast subject’s joint.`
- SRC-BOUND: Bound to / Stored at / Driven by. Help: `Where this camera’s eye comes from. It is sampled from its live render transform.`
- SRC-NOTE-OBJ: `The source is sampled from its live render transform. Prism does not hide marker geometry and does not use reflection probes.`
- SRC-NOTE-VIRT: `No prim is rezzed, so it works in no-rez parcels.`
- SRC-NOTE-SKEL: `A prim-free camera that rides a posed skeleton: yours, a cast subject, or a clone.`
- SRC-LENS-TYPE: Type = `Surface lens`. Help: `A Surface Lens looks through one aperture face. The type is fixed; add a camera for a different source.`
- SRC-LENS-APER: Aperture. Help: `The world face this lens looks through.`
- SRC-LENS-LOCATE: Locate aperture. Help: `Flash the aperture face in-world.` Status: `Flashing the aperture face of Cam <N> in-world.`
- SRC-LENS-NOTE: `A Lens owns exactly one aperture face. Its mapping is fixed; remove the capture to remove that face.`

Placement
- Card: `Placement`. PL-SET: `Set selected camera`; help `Bind this camera to the object you have selected in-world.`; status `Cam <N> bound to the selected object.`
- PL-FRONT: `Place eye in front`; help `Move the eye in front of the selected object.`; status `Cam <N>: eye placed in front of the object.`
- PL-NOTE: `Marker convention: local -Z looks forward and local +Y is up. Use a transparent marker or move the eye beyond visible housing.`
- PL-EX: Eye offset X (m); help `Local eye offset in metres (object cameras only).`
- PL-EY: Eye offset Y (m); help `Local eye offset in metres, +Y is up.`
- PL-EZ: Eye offset Z (m); help `Local eye offset in metres, -Z looks forward.`
- PL-SNAP: `Snap to my view`; help `Store your current camera position and orientation as this virtual camera’s transform.`; status `Cam <N> snapped to your current view.`
- PL-NOTE-V: `Fly your view to the shot, then Snap. The eye offset does not apply to virtual cameras.`

Skeleton POV
- Card: `Skeleton POV`.
- BP-ANCHOR: Anchor subject; help `Whose skeleton the camera follows.`
- BP-JOINT: POV joint; help `Eyeline or any joint from the anchor skeleton. Off-spine joints default to Stabilized aim.`
- BP-AIM: Aim mode; help `Full-follow drives orientation; Stabilized drives position only and leaves your aim untouched.`
- BP-ROLL: Roll mode; help `Horizon-lock keeps the horizon level; Inherit roll copies the joint roll.`
- BP-SMOOTH: Smooth (s); help `Exponential position and orientation damping; zero snaps.`
- BP-STATUS: Status; help `Live attachment state.`
- BP-OX / BP-OY / BP-OZ: Offset X / Y / Z; helps `Offset in metres, joint-local X.` / `... Y.` / `... Z.`
- BP-TPITCH: Trim pitch; help `Aim trim, pitch, in degrees.` BP-TYAW: Trim yaw; help `Aim trim, yaw, in degrees.`
- BP-SCALE: Clone-scale aware; help `Scale the joint point about the avatar foot and the local offset by the clone’s uniform scale.`
- BP-RESET: Reset attach; help `Restore every skeleton attachment option to its default.`

Optics and surface lens
- Card: `Optics`. LN-EQ: Lens equiv.; help `Full-frame equivalent focal length from the vertical field of view (24 mm sensor height).`
- LN-FOVMODE: Field of view; help `Fixed uses Degrees below; Follow projector takes it from the projector.`
- LN-DEG: Degrees; help `Vertical field of view in degrees.`
- LN-FROMPROJ: Degrees = `From projector (<deg>°)`; help `The projector currently supplies the field of view.`
- LN-FOVATT: FOV (attached); help `Vertical field of view while attached to the skeleton. Replaces Field of view.`
- LN-NEAR: Near clip; help `Near clipping plane distance.`
- LN-FAR: Far clip; help `Far clipping plane distance.`
- Card: `Surface lens` (global). LN-ZOOM: Lens zoom; help `Magnification shared by every Surface Lens (PrismLensZoom).`; note `Lens zoom applies to all lenses, not just this one.` (LN-ZOOM-NOTE)

In-world guide
- Card: `In-world guide`. GD-SHOW: Show guide; help `Draw a client-side wireframe frustum at this camera. No prim is rezzed; nothing is drawn while the UI is hidden (filming).`
- GD-THIRDS: Rule of thirds; help `Subdivide the framing gate into a 3x3 grid.`
- GD-UPROLL: Up / roll; help `Draw an up/roll nub on the top edge of the gate so tilt reads.`
- GD-CROSS: Center crosshair; help `Draw a small crosshair at the gate centre.`
- GD-CLIP: Clip range markers; help `Draw the near-clip rectangle so the depth range reads.`
- GD-NOTE: `Shows where this camera is and what its lens covers, so you can place it in the world.`

Look
- Card: `Camera look`. LK-EXP: Exposure EV; help `Exposure offset for this camera’s picture, separate from any face brightness.`
- LK-GRAIN: Film grain; help `Animated film grain over the picture.`
- LK-CA: Chromatic ab.; help `Colour fringing toward the frame edges.`
- LK-CRT: CRT lines; help `Scanlines baked into this camera’s picture (every face gets them).`
- Card: `Where this look goes`. LK-FEEDS: Feeds; help `Every face bound to this camera receives this look.`; empty value `No faces yet`.
- LK-NOTE: `These change the camera’s picture itself. Per-screen TV effects (roll, static, flips, a single monitor’s scanlines) belong to one face in 4 · Output.`
- LK-GOOUT: `Go to Output`; help `Open the Output section.`

Output
- Card: `Output frame`. OF-ASPECT: Output aspect; help `Shape of the picture this camera renders.` Options `16:9`, `4:3`, `1:1`, `Custom`.
- OF-CUSTOM: Custom W/H; help `Width over height.`
- OF-PIC: Picture; help `Logical axes are capped at 1024 px; Quality ceiling and Adaptive scale it.` Sample value `1024 × 576 px (scale 1.00)`.
- Card: `Faces this camera feeds` (see D.3).
- Card: `Face mapping · <face>` (scope This face). MP-FEED: Feed from; help `Route this face to the Gate program feed instead of its fixed camera.`; options `This camera`, `Gate program`.
- MP-FIT: Mapping; help `Fit keeps the whole picture with bars; Fill crops by the anchor; Stretch maps the complete picture.`
- MP-ANCX: Anchor X; help `Which part survives the crop, left to right.` MP-ANCY: Anchor Y; help `Which part survives the crop, top to bottom.`
- MP-SHAPE: Face shape; help `The surface the picture is mapped onto.`
- MP-HEIGHT: Height (m); help `Height of this virtual screen in metres; width follows the aspect.`
- MP-SCAR: Screen aspect; help `Width = height × aspect. Custom keeps the current ratio.`
- MP-LOCATE: Locate; help `Flash this face in-world.` Status `Flashing <face> in-world.`
- MP-REPOS: Reposition to my view; help `Move this virtual screen a few metres in front of your view, facing you.` Status `<face> moved in front of your view.`
- MP-REMOVE: Remove face; help `Unbind this face from the camera.` Status `Removed <face> from Cam <N>.`
- MP-GATE-NOTE: `This face shows whatever the Gate has on air, not Cam <N>. The mapping and TV effects below still apply.`
- MP-BARR: Bar R; help `Letterbox bar colour (linear), red. Bars appear with Fit.` MP-BARG: Bar G; help `Letterbox bar colour (linear), green.` MP-BARB: Bar B; help `Letterbox bar colour (linear), blue.`

TV effects (card `TV effects · <face>`)
- TV-PRESETS buttons: `Clean` (help `Zero every screen effect on this face.`), `Old CRT` (`Scanlines, vignette and a gentle flicker.`), `Broken TV` (`Rolling monochrome picture with static and dropouts.`), `VHS Tape` (`Tracking tears, chroma bleed and interlace shimmer.`), `Security Feed` (`Grayscale, coarse pixels, scanlines and vignette.`).
- Preset status: `Cleared every TV effect on <face>.` / `Applied <preset> to <face>.`
- TV-SCAN: Scanlines; help `Scanline strength on this face; 0 leaves the picture untouched.`
- TV-GRAY: Grayscale; help `Desaturate toward monochrome.`
- TV-STATIC: Static Noise; help `Animated analog snow.`
- TV-VIG: Vignette; help `CRT-style darkening toward the edges.`
- TV-BRIGHT: Brightness; help `Linear gain for this face only; separate from the camera’s Exposure EV.`
- TV-FLIPH: Flip H; help `Mirror this face’s picture left to right.`
- TV-FLIPV: Flip V; help `Mirror this face’s picture top to bottom.`
- TV-ROT90: Rotate 90; help `Quarter-turn the feed for portrait monitors (non-square faces show it stretched).`
- TV adv (14 rows), label = help:
  - TV-LINES Lines: `Dark/bright line pairs across the picture height (20–1200).`
  - TV-DARK Darkness (%): `Maximum gap darkening; 100 reaches black at full Scanlines.`
  - TV-SHARP Sharpness (%): `Blend the soft sine into crisp-edged bands.`
  - TV-THICK Line thickness (%): `Bright-line duty as a percentage of each pair.`
  - TV-PIXEL Pixelate: `Quantize into low-resolution blocks.`
  - TV-SEPIA Sepia Tint: `Blend toward a warm archival sepia.`
  - TV-CHROMA Chroma Bleed: `Horizontal colour fringing, like a worn composite signal.`
  - TV-VROLL Vertical Roll: `Lost V-sync: the picture rolls with a dark band.`
  - TV-ROLLSPD Roll Speed: `How fast the roll travels; only matters while Vertical Roll is above 0.`
  - TV-TRACK VHS Tracking: `Horizontal tearing on random rows.`
  - TV-FLICK Flicker: `Whole-picture brightness pulsing.`
  - TV-INTER Interlace: `Alternating field-line shimmer.`
  - TV-DROP Dropout: `Momentary signal loss.`
  - TV-SHEEN Sheen: `Glossy Fresnel reflection of the room over the feed.`
- RAW-NOTE: card `Showing the raw camera output`; note `Cam <N>’s monitor shows the picture before any face mapping. Pick a face above to edit its mapping and TV effects; the monitor then shows exactly what that face displays.`

Picture rate and performance
- Card: `Picture rate`. RT-MODE: Mode; help `Automatic lets the scheduler pick the cadence; Target FPS sets a ceiling. Adaptive may deliver less.`; options `Automatic`, `Target FPS`.
- RT-FPS: Target FPS; help `Requested ceiling for this camera.`
- RT-PRESET: Preset; help `Common delivery rates.`; options `Pick…`, `5`, `10`, `12`, `15`, `20`, `23.976`, `24`, `25`, `29.97`, `30`.
- RT-READOUT: Req/Ent/Obs; help `Requested / Entitled / Observed. Every face on this camera samples the same retained picture.`
- RT-NOTE variants: preview `No visible face: this camera renders only for its monitor, at the preview speed.`; idle `No visible face and previews are off: this camera is idle and costs nothing.`; default `Lower picture FPS saves auxiliary scene renders. Faces still composite the retained picture every main frame.`
- Card: `Performance` (global). PF-PROTECT: Protect main view; help `Reduce Prism scale or cadence, or suspend captures, to protect the main-view target.`
- PF-MAINFPS: Main-view target; help `Presented main-view FPS that Adaptive protects.`
- PF-BUDGET: Capture budget; help `Total capture budget: attempts per second shared by all cameras. Not a camera’s picture FPS.`
- PF-QUAL: Quality ceiling; help `Resolution scale ceiling for every capture.`
- PF-ADMIT: Admitted; help `Attempts the scheduler is admitting across all cameras right now.` Value: `<x> of <budget> attempts/s`. Mockup sample: `30.0 of 30 attempts/s` (entitled 63 capped at budget 30).
- PF-PREV: Previews; help `Monitors only cost renders for cameras with no visible face. Switch them at the top of the page.` Values: `Off (no cost)`, `On · <fps> fps · <n> attempts/s`, `On · <fps> fps · free`.
- PF-NOTE-1: `Adaptive protects presented main-view FPS and fairly shares the total budget across cameras. A camera’s Target FPS stays a ceiling; Entitled and Observed may be lower.`
- PF-NOTE-2: `Adaptive reacts to presented FPS only. It cannot attribute frame time to Prism or guarantee the target in an already slower scene.`
- PF-NOTE-3: `One camera feeding many faces still performs one scheduled scene render. Each visible face adds a smaller composite cost, so faces are capped at 16.`

### D.5 Step intros (one per chip; `<label>` = `Cam <N> (<Name>)`)

- Source: `What <label> sees: where its eye is and what drives it.`
- Lens: `How <label> frames it: field of view, clip range and the in-world guide.`
- Look: `Changes to <label>’s picture itself. They reach every face it feeds.`
- Output: `Where <label>’s picture goes. Choosing a face here switches its monitor to that face and opens the face’s mapping and TV effects.`
- Rate: `How often <label> renders, plus the budget every camera shares. Cards marked All cameras are global.`

### D.6 Status messages (status line, from mockup handlers)

Literal: `Virtual Cam is on.` / `Virtual Cam is off. Nothing renders; cameras and faces stay editable.` / `Monitor previews on at <R> fps.` / `Monitor previews off. No preview rendering; faces in-world keep updating.` / `Monitor previews refresh at <R> fps.` / `Sixteen faces is the maximum across all cameras. Remove one first.` / `Sixteen faces is the maximum across all cameras.` / `Eight captures is the maximum (<n> used).` / `Added Cam <N> and Cam <M>. It has no faces yet, so it renders only for its monitor preview.` / `Removed Cam <N> and its face bindings.` / `Editing Cam <N>.` / `Send Cam <N> to a surface: select a prim face in-world, or move an existing face.` / `Send to surface cancelled.` / `Cam <N>’s monitor now shows <face>.` / `Bound <face> to Cam <N>. Its monitor now shows that face.` / `<face> already shows Cam <N>.` / `Moved <face> from Cam <N> to Cam <M>. A face shows exactly one camera.` / `Added <face> to Cam <N>. Its monitor now shows that face.` / `Reset <card title>.` / `<Name>: source is now an object camera.` / `… a virtual camera (no prim).` / `… a skeleton POV (virtual camera driven by a joint).` / `Cam <N> bound to the selected object.` / `Cam <N>: eye placed in front of the object.` / `Cam <N> snapped to your current view.` / `Flashing the aperture face of Cam <N> in-world.` / `Flashing <face> in-world.` / `<face> moved in front of your view.` / `Opening the full Virtual Cam Manager (Captures, Displays, Performance, Gate).`

### D.7 Sample state (mockup seed data, for fixtures)

Cameras: Cam 1 `Street wide` (object, bound `Dolly rig (prim)`, Target FPS 24, FOV 55); Cam 2 `Close-up` (virtual, stored at `128.4, 96.2, 23.7`, FOV 22, film grain 0.25); Cam 3 `Hand POV` (skeleton, anchor Subject A, joint mWristRight, Stabilized, FOV 64); Cam 4 `Overhead drift` (virtual, stored at `140.0, 102.5, 61.0`, FOV 70, exposure -0.3, no entitlement).
Faces: `Lobby TV` (Cam 1, prim, 16:9, Visible), `Kiosk (portrait)` (Cam 1, prim, 9:16, Visible, Fill, anchor X 0.6), `Bar monitor` (Cam 2, prim, 4:3, Visible, Old CRT values), `Virtual screen 1` (Cam 3, virtual, 16:9, Offscreen, Fill).
New-object default name: `New object cam`; virtual: `Virtual view`; lens: `Surface lens`; OTS: `OTS A`, `OTS B`. New prim face: `Face <n> (selected prim)`, virtual: `Virtual screen <n>`, dropped: `Prim face (dropped)`.

---

## E. Discrepancies and blockers for VC6

1. **Card order.** Spec 6.1 says the first full-span card holds the monitor strip. The mockup and spec section 1 put the master card above it. Decide whether the master is a card or the page master plus status rows.
2. **Segmented controls.** The generator has no segmented kind. Its native kinds are `slider`, `number`, `toggle`, `combo`, `color`, `readout`, `button`, `text`, `list`, `dial`, `arc` (`director_kit_gen.py:48-51`). Affected rows: MON-LC, MON-SPEED, SRC-TYPE, MP-FEED. The current data uses `k: combo, ty: radio` (rendered as a dropdown), which does not match the mockup.
3. **Custom kinds are rejected.** `data_kind` maps `k: custom` through `CUSTOM_KIND`, and `control_for` raises `kit-native custom ... forbidden in P0` for unmapped types (`director_kit_gen.py:395-398, 442-443`). The monitor strip, tiles, pager, chips, and face list need a generator extension.
4. **Combo values are indices.** Items get `value = index` (`:464-465`). PF-MAINFPS (30/45/60), PF-BUDGET (0/5/10/15/20/30), and MON-SPEED (0.5/1/2/5) cannot be expressed. RT-PRESET's 23.976 and 29.97 cannot be set on the integer target FPS slider.
5. **No conditional visibility.** Type variants, Fill-only anchors, virtual-only rows, FOV mode, Custom aspect, Target-FPS-only rows, Gate note, raw note, guide sub-rows, and the per-face cards all need show-if syntax. The data format has none. `kit_page.mode` covers only Easy/Advanced.
6. **Scope badges.** "All cameras", "This face", and "Cam N" badges have no data key. The spec requires global cards to be visibly labelled.
7. **Chips versus master and monitors.** Kit sections are the five step chips. The master and monitor cards sit above them in the mockup and in spec 6.1, but cards outside any section have no defined placement in `kit_page`.
8. **Tile states.** Spec 4.1 defines TileState as EMPTY, READY, HELD, THROTTLED, PAUSED, ERROR. The mockup shows Live, Preview only, Idle, Paused, and an off overlay. OFFLINE (spec 4.3) and WARMING (spec 4.5) are absent from the mockup. Mapping is UNDEFINED IN SPEC.
9. **Send-to-surface menu.** Spec 5.3 lists "Bind selected prim face", "Move existing face...", "Cancel". The mockup has "Edit settings", "Send to surface…", "Show raw output" on the tile, and "Use selected in-world face", "Move an existing face here…", "Cancel" on the drop bar. The strings differ; the mockup text is used in section D.
10. **Capacity text.** Spec 6.4 says "N of 8". The mockup uses "4/8 cameras" in the summary and "8 of 8" at cap. Face capacity is "N of 16 faces used (all cameras)".
11. **Stale right rail.** "Cards 1 / Sections 1 / Controls 3 / Presets saved only" comes from the old one-card VCam page. Recount for the new page.
12. **Default state.** The mockup starts with Virtual Cam on. `PrismLensEnabled` defaults to 0 (`settings.xml:29476-29481`). Previews default to on at 1 fps in the spec (`PrismMonitorPreviewsEnabled`, `PrismMonitorPreviewFPS`), but neither key exists in settings.xml yet (VC1).
13. **Missing spec rows.** Output age is missing from the Req/Ent/Obs readout (spec 6.3). The Previews switch and Speed control are not badged "All cameras" in the mockup, though spec 6.3 says preview master and rate are global.
14. **Spec anchors drift.** Spec 3.1 cites `settings.xml:29432-29438` for PrismLensEnabled; the current anchor is 29476. Values match, only line numbers moved.
15. **Lens help copy.** The mockup Large help says "scroll sideways", but spec 4.4 defines paging (1-4 / 5-8). Use the spec behaviour and fix the copy.
16. **Attempt-budget default.** Mockup `total_capture_budget` default is "30 attempts/s" and matches `PrismCaptureRefreshCeilingHz` = 30. The "Adaptive 30 / Manual every frame" label maps to no single stored value (spec: 0 = Every Frame in Manual mode).

---

## F. Counts (verify with grep on this file)

- Rows in section B: **126** (IDs listed in B.1 to B.10, one per row).
- Custom rows (`custom` in the Widget column): **5** (MON-STRIP, TILE, MON-PAGE, SEL-CHIPS, FACE-LIST).
- UNDEFINED IN SPEC: **27** (12 non-copy: MON-FOOT, SRC-TYPE, PL-FRONT, LN-EQ, LN-FROMPROJ, LK-FEEDS, LK-GOOUT, OF-CUSTOM, OF-PIC, MP-SHAPE, RT-PRESET, PF-ADMIT; 15 copy-only notes).
- Status: D = 66 (4 of them custom), P = 33 (TILE included), UNDEFINED IN SPEC = 27. Total 126 = 66 + 33 + 27.
- Setting-bound drafts in section C.2: **8** (MST-SW as page master, MON-PREV, MON-SPEED, LN-ZOOM, PF-PROTECT, PF-MAINFPS, PF-BUDGET, PF-QUAL). Of these, 3 are BLOCKED on generator support (MON-SPEED, PF-MAINFPS, PF-BUDGET).
