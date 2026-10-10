# Director's Camera / VCam Monitors Design

Status: implementation design for work after Director Kit Phase 0  
Branch reviewed: `feature/director-kit-p0`  
UI reference: approved `VCamPage.dc.html` mockup  
Scope of this document: the inline Director **Lens > VCam** page, its preview engine contract, and drag-to-surface. The existing full Virtual Cam Manager remains available as an advanced/debug surface.

## 1. Decision summary

Build the page as a shared, build-once `ALPanelDirectorVCam` embedded in the Director floater. It owns a fixed set of eight `ALVCamMonitorTile` children, a camera selector, and the Source / Lens / Look / Output / Rate & performance editors. It does not recreate widgets or query controls by name after `postBuild()`.

Add one engine-side `LLPrismPreviewService` beside the existing Prism registry. It does **not** introduce a second scene renderer:

- A capture already rendering for at least one visible display reuses its retained `gPipeline.mPrismLensOutput[slot]`; no extra scene render is admitted for its monitor.
- A capture with no visible display may become preview-demanded. Its next output is rendered through the existing `renderAuxiliaryView()` path, existing adaptive controller, and existing `chooseRenderSlot()` arbitration, but at a preview-only logical size and at no more than the shared preview rate.
- The existing scheduler still admits at most one auxiliary capture attempt in a presented frame. Preview-only captures participate in its round-robin/fairness cursor rather than owning another timer or render loop.
- A fixed eight-cell RGBA8 preview atlas is updated from retained capture output. One cell update applies the same signal mapping and effect math used on an in-world display. The UI only samples this atlas; it never samples a live render target whose lifetime can change under it.
- The Previews master switch removes all preview demand and schedules no atlas updates. In-world displays continue at their own picture rates. Existing retained atlas pixels may be drawn dimmed with a PAUSED badge, but they are not refreshed.

The page has two independent notions of choice:

1. `mSelectedCapture` chooses which camera the editor changes.
2. `mMonitorDisplay[capture-id]` chooses what that capture's monitor shows: raw signal or one of its display bindings.

Clicking a monitor never changes the edited camera. A monitor is an output view and drag source, matching the approved mockup.

## 2. What exists today

The following facts are load-bearing and are the reason for the design above. Line numbers are anchors on `feature/director-kit-p0` and will drift as work lands.

- Capacity is already split correctly: 8 captures and 16 display bindings (`llprismlens.h:27-31`). A capture owns output; displays only sample it.
- Capture and display definitions, including configuration/runtime revisions, are already exposed by `RegistrySnapshot` (`llprismlens.h:477-509`). Captures do not have a user label.
- A display's Fit / Fill / Stretch, anchor, linear bar color, flips/rotation, and all TV effects live in `DisplaySettings` / `ScreenEffects` (`llprismlens.h:365-475`). These are per-display, not per-capture.
- TV effects are not baked into the retained capture. They are packed while composite states are built (`llprismlens.cpp:7719-7757`) and executed in `app_settings/shaders/class1/deferred/prismLensF.glsl:64-206` and later effect blocks. Mapping is built at `llprismlens.cpp:7770-7818`.
- Each capture has a lifetime-fixed 1024 x 1024 RGBA16F retained output allocated lazily (`pipeline.cpp:1828-1856`). Logical output size and UV region are separate, so a preview can reuse the texture without copying or reallocating it.
- Auxiliary rendering prepares every capture, calls `chooseRenderSlot()` once, and renders only that slot (`llprismlens.cpp:7164-7241`). It uses exact-size pooled deferred scratch targets and copies the completed HDR beauty into the retained output (`llprismlens.cpp:7248-7259`, `7585-7628`; scratch allocation is `pipeline.cpp:1728-1825`).
- A camera feed is currently prepared only if a display is visible (or a short Gate prewarm is active); otherwise it exits at `llprismlens.cpp:2876-2886`. The scheduler likewise treats only `mAnyDisplayVisible` captures as active (`llprismlens.cpp:3041-3156`, `3159-3249`). This is the exact seam preview demand must extend.
- The adaptive controller is updated once before preparation and protects the main view by reducing scale/cadence or suspending work (`llprismlens.cpp:259-523`, `7169-7177`). Preview work must remain under it.
- The main and auxiliary display composite paths bind the retained output and upload the same mapping/effect uniforms (`pipeline.cpp:26581-26838`; `llprismlens.cpp:7632-7861` and `7895-8076`).
- The manager already provides add camera, add surface lens, add virtual camera/screen, remove, per-capture rate, display mapping, and display effect handlers (`llfloaterprismmanager.cpp:1778-2425`). These handlers should move behind a reusable page model rather than be copied.
- The current manager polls snapshots from `draw()` every 0.25 seconds and rebuilds lists on revisions (`llfloaterprismmanager.cpp:705-777`, `953-1060`). The current Director summary also polls from its refresh path (`llfloaterdirector.cpp:3408-3533`). The new page must replace this with signals/coalesced refresh and obey `CLAUDE.md`'s UI draw-cost rules.
- The current Director VCam tab is only a small enable/summary/open-manager card (`floater_director.xml:1342-1353`).
- Bone POV is a per-frame driver for virtual Camera Feed settings. `ALVCamBonePov::tick()` snapshots captures and writes runtime-only transforms through `setVirtualCameraPosition/Transform`; its status API is in `alvcambonepov.h:20-27`. The page edits its `CameraSettings::mBonePov`; it does not add another driver.
- Current face binding begins from the current `LLSelectMgr` selection (`llprismlens.cpp:1177-1275`) and is validated/created by `displaySelectionStatus()` / `addSelectedDisplay()` (`llprismlens.cpp:1825-1881`, `2017-2058`). It is viewer-local and does not change a prim, TE, or material (`llprismlens.h:580-581`).
- Drag cargo types are global in `indra/llui/llui.h:54-82`; 3D dispatch is registered in `lltooldraganddrop.cpp:211-236`. `SOURCE_VIEWER` exists, but the root/top-view traversal currently forwards non-inventory UUID cargo only for `SOURCE_PEOPLE` (`lltooldraganddrop.cpp:499-620`), so viewer-owned VCam cargo needs an explicit forwarding case.

## 3. Ownership and data model

### 3.1 Existing scopes

| Scope | Data / setting | Existing write path |
|---|---|---|
| Global | VCam master | `PrismLensEnabled` (`settings.xml:29432-29438`) |
| Global | Surface Lens zoom | `PrismLensZoom` (`settings.xml:29474-29480`) |
| Global | Adaptive protection | `PrismAdaptivePerformance` (`settings.xml:29502-29508`) |
| Global | Protected main FPS | `PrismProtectedMainFPS` (`settings.xml:29509-29515`) |
| Global | Total attempt budget | `PrismCaptureRefreshCeilingHz` (`settings.xml:29516-29521`) |
| Global | Quality ceiling | `PrismLensResolutionScale` (`settings.xml:29481-29487`) |
| Capture | Source, FOV, clips, aspect, guide, optics, virtual transform, Bone POV | `setCameraSettings()` (`llprismlens.cpp:3515-3556`) |
| Capture | Automatic / Target FPS | `setCaptureRateSettings()` (`llprismlens.cpp:3641-3667`) |
| Display | Fit / Fill / Stretch, anchor, bars, virtual screen transform/size, TV effects | `setDisplaySettings()` (`llprismlens.cpp:3670-3746`) |
| Display | This capture / Gate program | `setDisplayGateSubscribed()` (`llprismlens.h:650-653`) |

### 3.2 New state

Add the following:

```cpp
// llprismlens.h
struct CaptureDefinition
{
    // existing fields...
    std::string mLabel; // UTF-8, trimmed, maximum 64 bytes at a codepoint boundary
};

bool setCaptureLabel(const CaptureHandle&, const std::string&, std::string* reason = nullptr);

// settings.xml; both global/user-persistent, not Director scene data
PrismMonitorPreviewsEnabled = true
PrismMonitorPreviewFPS = 1.0 // accepted values: .5, 1, 2, 5
```

`PrismInstance::mLabel` is copied into snapshots. `sceneData()` writes optional `label`; `applySceneData()` accepts its absence and generates a default, preserving old scenes. Defaults are `Camera N`, `Surface Lens N`, `Virtual N`, and `OTS A` / `OTS B`. Labels need not be unique; identity remains UUID + generation. OTS replacement continues to use its existing arm-label ownership marker, not the user-facing label.

The following is UI-session state, deliberately not scene state:

```cpp
CaptureHandle mSelectedCapture;
std::array<DisplayHandle, MAX_CAPTURES> mMonitorDisplay; // null means raw
EEditorSection mSection;
```

The display choice is cleared to raw if its handle disappears or moves to another capture. It does not change the in-world feed or Gate routing.

### 3.3 Reusable page model

Introduce `ALVCamPageModel` (`alvcampagemodel.h/.cpp`) as the only mutation surface used by both the inline panel and, incrementally, the full manager. Its responsibilities are:

- Hold selected capture/display handles and a cached `RegistrySnapshot`, `PerformanceSnapshot`, and `GateSnapshot`.
- Expose typed commands: `addFromSelectedObject`, `addVirtualAtView`, `addOtsPair`, `addSurfaceLens`, `removeCapture`, `bindSelectedFace`, `bindFaceTarget`, `reassignDisplay`, `setCamera`, `setRate`, `setDisplay`, and `setLabel`.
- Open/close an undo transaction around one user gesture. Slider mouse-down/up is one transaction; a drag/drop, Add, Remove, preset, or reset is one transaction.
- Preserve the existing validation and status strings by calling Prism registry APIs, never duplicating their rules in the panel.
- Emit `changed(DirtyBits)` after coalescing engine signals. The panel performs widget writes only from this callback/idle work item and only while visible.

This model is not a second source of truth. Handles and snapshots remain owned by `LLPrismLens`.

## 4. Monitor preview engine

### 4.1 Classes and public contract

Add `llprismpreview.h/.cpp`:

```cpp
namespace LLPrismPreview
{
struct MonitorSelection
{
    CaptureHandle mCapture;
    DisplayHandle mDisplay; // null = raw
};

struct AtlasHandle
{
    U64 mGeneration = 0;
};

struct TileState
{
    LLRectf mAtlasUV;
    U64 mPublicationSerial = 0;
    EPreviewState mState; // EMPTY, READY, HELD, THROTTLED, PAUSED, ERROR
};

void setPanelVisible(bool);
void setMonitorSelection(U32 capture_slot, const DisplayHandle&); // null = raw
AtlasHandle atlasHandle();
bool bindAtlas(const AtlasHandle&); // resolves/binds only on the render/UI thread
TileState tileState(const CaptureHandle&);
U64 revision();
boost::signals2::connection connectChanged(...);
}
```

`LLPrismPreviewService` owns one fixed 4 x 2 atlas. Each cell is 256 x 144; the texture is 1024 x 288 RGBA8, no mipmaps. It is allocated once on first visible use, reused until GL targets are released, and recreated by `onRenderTargetsReleased()`. Fixed cells mean list reorder, large/compact mode, and capture selection never allocate GPU memory. Slot identity is checked with capture generation before publishing into a cell.

The atlas handle is an opaque generation token, not a GL name. `bindAtlas()` resolves and binds the service-owned target only during `ALVCamMonitorTile::draw()` on the viewer render/UI thread. The generation changes before release/recreate. A tile with a stale generation draws EMPTY; it never retains `LLRenderTarget*`, `LLViewerTexture*`, or an unguarded GL name across context loss.

### 4.2 Demand and cadence

Extend `PrismInstance` with runtime-only preview fields:

```cpp
bool mPreviewDemanded = false;
bool mPreviewOnly = false;
F64  mNextPreviewDueTime = 0.0;
F32  mPreviewEntitlementHz = 0.f;
U64  mPublicationSerial = 0;
```

At the start of `renderAuxiliaryView()`, after the adaptive update and before `prepare()` (`llprismlens.cpp:7174-7219`), the service publishes demand for every occupied capture only when all of these are true:

- `PrismLensEnabled` is true.
- `PrismMonitorPreviewsEnabled` is true.
- the inline panel or full manager monitor strip is in the visible chain;
- the shared preview deadline is due;
- the capture has no prepared visible display.

Refactor `prepareCameraCapture()` (`llprismlens.cpp:2774-2921`) so the source/health/FOV checks remain common, then choose target demand:

1. visible display/Gate demand: use the existing required size and existing capture rate;
2. preview-only demand: fit the capture aspect inside 256 x 144, clamp each axis to the existing minimum extent, and mark `mPreviewOnly`;
3. no demand: return false exactly as today.

`updateCadenceEntitlements()` continues to own the single global attempt budget. For a preview-only capture its demand is:

```text
min(shared preview FPS, capture Target FPS when Target FPS is active)
```

Automatic mode uses the shared preview FPS. Preview demand is included in the existing max-min water filling and adaptive cadence factor; it is not a free side channel. The runtime snapshot exposes preview requested/entitled/observed values so the UI can say that 1 fps was requested but, for example, 0.5 fps was admitted.

`chooseRenderSlot()` remains the only admission point and therefore the existing “one auxiliary attempt per frame” invariant remains true. Due preview-only captures use the same `mNextRenderSlot` round-robin cursor and the same overdue/attempt-age ordering (`llprismlens.cpp:3159-3249`). Do not call `renderAuxiliaryView()` a second time and do not add a panel timer that renders.

On `markProduced()` (`llprismlens.cpp:3252`), increment `mPublicationSerial` and signal the preview service. A visible capture can update its atlas cell on the next shared preview deadline. A preview-only capture updates its cell from the just-produced retained output. At most one atlas cell is composited per frame; a separate `mNextAtlasSlot` gives deterministic round-robin service to retained outputs.

Turning Previews off performs, synchronously on the main thread:

- clear all `mPreviewDemanded` flags;
- reset preview deadlines/tokens (no catch-up credit on re-enable);
- cancel queued atlas cell updates;
- bump preview revision so tiles show PAUSED.

It does not clear capture publications and does not disturb visible-display cadence. Thus “Previews off” means **zero preview-triggered scene renders and zero preview atlas renders**, not “zero Prism renders”; surfaces in-world still update.

### 4.3 Raw and face-output atlas composition

Do not approximate the signal effects in C++ or LLUI. Extract the mapping, capture optics, orientation, and TV-effect body from `prismLensF.glsl` into a linkable fragment helper, for example `deferred/prismSignalF.glsl`, with one function:

```glsl
vec4 prismSignalSample(vec2 surface_uv,
                       vec3 eye_pos, vec3 eye_normal,
                       /* existing mapping/effect uniforms */);
```

OpenGL permits multiple shader objects of the same stage as long as only one supplies `main()`. Register the helper plus the existing `prismLensF.glsl` for `gPrismLensProgram`, and the same helper plus a small `prismPreviewF.glsl` for new `gPrismPreviewProgram`. This avoids two implementations drifting. Shader creation is all-or-nothing: if the preview program fails, in-world Prism remains available and monitors show “Preview shader unavailable”; never substitute a silently different effect.

The raw monitor supplies identity display mapping, no bars, and zero `ScreenEffects`, but it still supplies the capture-owned `OpticsSettings`. This matches the current architecture, where Look is capture-owned even though its math occurs during composite.

For a chosen display, factor the common uniform packing/mapping math currently duplicated in `getCompositeStates()` and `getAuxCompositeStates()` into:

```cpp
bool buildSignalCompositeState(U32 capture_slot,
                               const PrismDisplay* display_or_null,
                               F32 display_aspect,
                               SignalCompositeState& out);
```

Both world composite builders and the preview service call it. `prepareDisplayFrame()` already resolves/synthesizes the surface basis before frustum rejection (`llprismlens.cpp:2617-2721`); retain the resolved world U/V edges and aspect even when the face is offscreen. The preview shader reconstructs the same per-pixel face eye position and normal from that basis and the current main-view modelview, so environmental sheen uses the same input as the world shader. If a real face is offline and has never supplied a valid basis, exact face output is unavailable: draw an OFFLINE slate rather than inventing an aspect or sheen result.

An atlas update is a scissored two-triangle draw into one cell. It binds the retained 1024 x 1024 output once, uses the published logical region transform (`llprismlens.cpp:7763-7768`), and writes display-referred RGBA8 for LLUI. It never reads pixels back and never allocates. Animated TV effects use the same `gFrameTimeSeconds` value as the world composite (`pipeline.cpp:26747-26754`) but advance only when that monitor's shared preview deadline is serviced, as a preview running at 0.5/1/2/5 fps should.

“Exactly that face's result” means exact signal-space mapping, bars, flips/rotation, capture Look, and TV-effect shader math from the same retained publication. It does not mean a screenshot of the face after world depth occlusion, transparency, bloom, or final main-view tonemapping; see Risks.

### 4.4 UI drawing

`ALVCamMonitorStrip` creates eight `ALVCamMonitorTile` children in `postBuild()` and only changes visibility, rectangle, label, atlas UV, and status. Layout is one row:

- 1-4 captures: four equal large slots; unused slots hidden.
- 5-8 captures: eight equal compact slots.
- Optional explicit Large mode pages 1-4 / 5-8, as in the mockup, changes geometry only and does not rebuild children.

`ALVCamMonitorTile::draw()` contains `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam monitor tile")`, validates the atlas generation, binds it through a small scoped LLUI texture adapter, and draws one textured quad plus cached text/icons. It performs no settings reads, UUID formatting, by-name child lookup, vector/string construction, or allocation. Labels/status strings are prepared by the model on revision callbacks. The page root `draw()` contains `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam page")` and then calls `LLPanel::draw()` only.

Add a render zone `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview atlas composite")` around the atlas cell update. Preview-only scene renders nest `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview-only auxiliary update")` inside the existing `"Prism one scheduled auxiliary update"` zone.

### 4.5 Performance budget and self-test

The default requirement is: with eight captures requesting 1 fps, monitor preview work contributes no more than 0.3 ms to the average presented frame. Count both preview-only auxiliary renders and preview-atlas composite GPU time; do not count ordinary capture renders required by visible world displays, because those exist with previews off.

Add a `PrismPreviewBudgetController` inside the preview service:

- Non-blocking timestamp-query rings measure preview-only auxiliary GPU time and atlas-composite GPU time. Results are consumed only when available; no query wait is allowed on the render thread.
- CPU wall time and Tracy zones are recorded separately for diagnosis.
- The rolling allowance is `0.3 ms * presented_frames` over a 2-second window. Completed measured preview work debits it. An attempt is admitted only if the remaining allowance covers its conservative predicted cost.
- Prediction is the maximum of a cold-start seed and the capture's rolling p95. The seed is 2.0 ms for a 256 x 144 auxiliary render and 0.05 ms for an atlas update. An over-budget result raises the prediction immediately; recovery decays slowly.
- The controller is subordinate to the existing global Prism attempt budget and adaptive protection. Both must admit the work.
- If budget is exhausted, preview-only renders and atlas updates are deferred without catch-up. The requested speed is a ceiling, not a promise. UI state becomes THROTTLED and shows effective preview fps.
- If an atlas draw alone cannot fit, cells retain their last image. If the condition persists, disable effects animation first only if the UI explicitly says “held”; never switch to different effect math while claiming an exact live preview.

This controller enforces the 0.3 ms admission envelope after timing is available: when completed measurements reach the rolling allowance, it admits zero further work until the window recovers. It cannot mathematically prevent an individual pass from costing more than its prediction and pushing the already-completed window over the line, nor can it bound an unknown first render on arbitrary hardware; a GPU pass cannot be pre-empted after submission. During cold start the UI and self-test report WARMING / INCONCLUSIVE, and any overrun immediately suspends further preview-only work until the rolling window recovers. The self-test is the release gate for the target hardware. This is the honest limit of the requirement.

Add non-persistent debug setting `PrismPreviewSelfTest` and console hook `LLPrismPreview::runSelfTest(seconds = 20)`. It requires the page visible, previews on at 1 fps, and eight valid captures; otherwise it logs `INCONCLUSIVE` with the missing condition. It records:

- presented frame count and wall duration;
- requested/admitted/produced preview-only renders per capture;
- reused-publication atlas updates;
- GPU sum and p95 for preview-only auxiliary and atlas work;
- `average_ms_per_presented_frame = preview_gpu_sum / presented_frames`;
- query coverage and any disjoint/unavailable timing interval;
- budget deferrals and achieved per-capture fps.

Verdict is `PASS` only with at least 95% GPU-query coverage, every capture serviced at least once, and average <= 0.300 ms. It is `FAIL` above 0.300 ms and `INCONCLUSIVE` for missing/unreliable timing. Log one machine-readable line:

```text
[VCamPreviewSelfTest] PASS avg_gpu_ms_per_frame=0.214 aux_p95_ms=1.72 atlas_p95_ms=0.04 frames=1204 coverage=0.99 deferred=7
```

A scheduler unit test injects synthetic costs and proves: one preview-only attempt per frame, round-robin progress, no catch-up, no demand while disabled/hidden, and rolling allowance never admits work above available tokens. The in-viewer hook validates actual GPU cost.

## 5. Drag-to-surface and Send to surface

### 5.1 Cargo and 3D dispatch

Append `DAD_VCAM_FEED = 22` and increment `DAD_COUNT` in `indra/llui/llui.h:56-82`. Never renumber existing cargo values. Register it in `LLDragAndDropDictionary` (`lltooldraganddrop.cpp:211-236`) with only `DT_OBJECT -> dad3dVCamFeed`; self, avatar, land, and none reject.

`ALVCamMonitorTile::handleMouseDown/hover` starts:

```cpp
LLToolDragAndDrop::instance().beginDrag(
    DAD_VCAM_FEED, capture.mId, LLToolDragAndDrop::SOURCE_VIEWER);
```

Teach both top/root UI traversal and 3D dispatch in `LLToolDragAndDrop::dragOrDrop()` to forward a UUID pointer for `SOURCE_VIEWER && DAD_VCAM_FEED`, analogous to person UUID cargo but with the actual `drop` flag on the second pass. No inventory lookup is performed. Registry UUID lookup returns the current generation-bearing handle; removal during a drag therefore becomes a stale-cargo rejection.

`dad3dVCamFeed(obj, face, mask, drop)` calls a new target-based Prism API. It returns `ACCEPT_YES_COPY_SINGLE` for a new binding, `ACCEPT_YES_SINGLE` for reassignment of an existing binding, and `ACCEPT_NO_CUSTOM` with the registry reason otherwise. Multi-drag is rejected.

### 5.2 One validation/bind path

Refactor, do not clone, current selection validation:

```cpp
ActionStatus displayTargetStatus(const CaptureHandle&, LLViewerObject*, S32 te,
                                 const DisplayHandle* reassigning = nullptr);
ERegistryResult bindDisplayTarget(const CaptureHandle&, LLViewerObject*, S32 te,
                                  EFitMode, DisplayHandle*, std::string*);
ERegistryResult reassignDisplay(const DisplayHandle&, const CaptureHandle&,
                                std::string*);
```

`selectedFaceIdentity()` becomes a selection adapter that finds `(object, te)` and calls `displayTargetStatus()`. Existing `addDisplaySelectionStatus()` and `addSelectedDisplay()` call the same target APIs. Drag/drop and “Send to surface...” therefore reuse the manager's code path and cannot diverge on geometry, feedback, capacity, or duplicate checks.

Validation on hover and again on drop requires:

- a live, static, non-HUD `LLVOVolume`, a valid face index, valid rectangular surface geometry, and no rigged/animated face (existing checks at `llprismlens.cpp:1244-1263` and `1868-1877`);
- the root edit object is modifiable by the agent (`permModify()`); own objects pass, group-owned objects pass only when effective group powers make `permModify()` true; other-owned and inventory-add-only objects reject;
- the face is not the destination capture's source object (existing feedback rule at `llprismlens.cpp:1862-1866`) and does not create Gate feedback;
- a new binding respects the global 16-display cap; reassignment is allowed at 16 because it does not allocate a display;
- a Surface Lens's required aperture display cannot be moved to another capture.

The permission rule is product policy, not a simulator security boundary: bindings remain viewer-local and send no object update. Apply it to the button path as well as drag so the two affordances agree.

Dropping on an unbound face creates a display with the page's “new face mapping” default. Dropping on a face already represented by a Prism display atomically changes its capture owner, preserving `DisplayHandle`, mapping, virtual/real identity, Gate subscription, and TV effects. It decrements/increments display counts, releases the old capture output only if the old capture has no display, Gate, or preview demand, bumps configuration revision once, and emits one change signal.

Dropping a VCam tile on a face row calls the same `reassignDisplay(display, capture)` API. The row is an LLUI drop target; it does not invoke a world pick.

### 5.3 Send to surface

The tile menu's **Send to surface...** opens a small anchored menu with:

- **Bind selected prim face**: calls `bindSelectedFace(capture, selected new-fit mode)` immediately, using the current exactly-one-face selection.
- **Move existing face...**: submenu of display bindings owned by other captures; calls `reassignDisplay`.
- **Cancel**.

If there is no eligible selected face, keep the command visible but disabled with the exact `displayTargetStatus` reason. Do not enter a modal “waiting for selection” mode; selection can change independently and the page refreshes its enablement from the selection signal.

### 5.4 Undo

Add bounded `ALPrismUndoHistory` (32 transactions). A transaction stores the before/after `LLPrismLens::sceneData()` plus selected capture/display handles and a label such as “Bind Camera 2 to face” or “Move Lobby screen to Camera 4”. Undo/redo calls the already-atomic `applySceneData()` and restores selection only if handles still exist.

Drag/drop, Send, Add, Remove, preset/reset, and a completed slider gesture go through `ALVCamPageModel::transaction()`. The VCam page exposes Undo/Redo buttons and claims Ctrl-Z/Ctrl-Shift-Z only while one of its controls has focus. Do not add a server object-edit undo entry: no simulator object was edited. Failed or no-op drops create no history entry.

## 6. Inline page and model binding

### 6.1 Files and layout

Add:

- `alpaneldirectorvcam.h/.cpp` — page controller and cached widget pointers.
- `alvcampagemodel.h/.cpp` — selection, commands, snapshots, dirty coalescing, undo integration.
- `alvcammonitorstrip.h/.cpp` — fixed monitor layout and tile LLViews.
- `skins/default/xui/en/panel_director_vcam.xml` — shared panel.

Replace `floater_director.xml:1342-1353` with a filename-based panel embed and no hand-authored embed height. The shared panel owns its scroll document, and the Director tab/scroll fitting is installed through `ALScrollFit::Controller::installForTabs`. The monitor strip stays above the scrollable editor. No code writes a scroll-document height.

### 6.2 Refresh lifecycle

Add Prism signals emitted on the main thread after mutation/publication:

```cpp
configurationChanged(U64 revision)
runtimeChanged(U64 revision)
performanceChanged(U64 revision)
previewChanged(U64 revision)
```

The page model also connects to the commit signals for the six relevant global controls and the world-selection change signal. Runtime/performance/preview publishers coalesce or rate-limit notifications to 10 Hz; configuration changes remain immediate. Callbacks only OR dirty bits and enqueue one main-idle refresh. They never rewrite widgets from the render path.

Refresh rules:

- configuration: rebuild the at-most-8 camera selector and at-most-16 face rows, preserving handles;
- runtime/performance: update cached readout values at no more than 10 Hz with `ALUIRefresh` diff guards;
- preview publication: update tile state/UV only; texture pixels are already in the atlas;
- selection: refresh Add/Send enablement only;
- hidden VCam tab: retain dirty bits and do no widget work; force one refresh on `onVisibilityChange(true)`;
- `draw()`: Tracy zone plus base draw only. No snapshot, settings lookup, string formatting, `setText`, `clearRows`, `add`, or by-name lookup.

This replaces the VCam part of `LLFloaterDirector::refreshPrismDependents()`; the OTS controls elsewhere may keep their own cheap revision handling until migrated. The full manager can initially subscribe to the same signals while retaining its UI, eliminating its 0.25-second draw poll without requiring the redesigned layout to be duplicated.

### 6.3 Row bindings in signal-chain order

**Source (capture)**

- Object source: current `mCameraObjectId`; **Set selected camera** -> `setSelectedCamera()` (`llprismlens.h:617-621`). Eye offset -> `CameraSettings::mLocalEyeOffset`.
- Virtual at my view: `CameraSettings::mVirtual`, `mVirtualPos`, `mVirtualRot`; **Snap to my view** uses a factored shared version of the manager's current-view transform logic (`llfloaterprismmanager.cpp:1907-1983`).
- Skeleton POV: `CameraSettings::mBonePov`; status from `ALVCamBonePov::status()`; all writes still go through `setCameraSettings()`.
- Surface Lens: fixed type/aperture identity; Locate selects/flashes its required display. It is not convertible in place.

**Lens (capture, except one labeled global control)**

- FOV mode/fixed vertical FOV, near/far clip, output aspect -> `CameraSettings`.
- Attached FOV -> `BonePovSettings::mFovDeg`.
- Guide and guide aids -> `CameraSettings::mShowGuide` and guide flags.
- Surface Lens zoom -> global `PrismLensZoom`, visibly badged **All cameras**.

**Look (capture)**

- Exposure, film grain, chromatic aberration, capture CRT lines -> `CameraSettings::mOptics`.
- The page states that these reach every display fed by the capture. They are capture-owned even though the current composite shader applies them while sampling retained output.

**Output**

- Capture output aspect stays in `CameraSettings::mOutputAspect`.
- Face list filters `RegistrySnapshot::mDisplays` by capture handle (and marks Gate-subscribed routing explicitly).
- Selecting **Camera output (raw)** clears `mMonitorDisplay[capture]`.
- Selecting a face sets `mMonitorDisplay[capture]`, opens mapping/effects, and changes only the monitor view.
- Feed source -> `setDisplayGateSubscribed()`.
- Fit, anchor, bar color -> `DisplaySettings` through `setDisplaySettings()`.
- Virtual screen height/aspect/reposition -> the existing `DisplaySettings::mVirtual/mPos/mRot/mWidth/mHeight` path.
- Presets and every TV control -> `DisplaySettings::mEffects`; existing preset values in `llfloaterprismmanager.cpp:279-319` become shared helpers.
- Locate, Remove, Add selected face, and Add virtual screen use existing engine calls via the page model.

**Rate & performance**

- Picture mode and target FPS -> per-capture `CaptureRateSettings`.
- Requested / Entitled / Observed / output age -> capture runtime snapshot, including preview-only state.
- Protect main view, main-view target, total capture budget, and quality ceiling -> existing globals, each badged **All cameras**.
- Preview master/rate are global and live only in the Monitors header; the performance card mirrors their effective cost readout but does not add a second control.

### 6.4 Add and Remove

The Add menu maps directly to current APIs:

- **From selected object** -> `addCameraCaptureFromSelectedObject()`.
- **Virtual at my view** -> factored current-view transform + `addVirtualCamera()`.
- **OTS pair** -> `buildOtsPairFromSubjects()`; it is one undo transaction and requires two available capture/Gate arm slots, preserving its current atomic rollback behavior.
- **Surface lens** -> `addSurfaceLensFromSelectedFace()`.

On success select the new capture (OTS selects A), assign its default label, select Source, and request a preview refresh. Capacity text is `N of 8`; OTS is disabled when only one slot remains. Remove uses the current confirmation semantics and `removeCapture()`, which removes all display bindings. Selection moves to the next surviving capture, otherwise the page shows its empty state.

## 7. Not feasible as stated, and risk controls

1. **An absolute 0.3 ms guarantee before measuring the GPU is not feasible.** A single new scene render has unknown cost and cannot be cancelled after submission. The implementable contract is a conservative cold-start seed, non-blocking measurement, hard rolling admission after measurement, immediate suspension on an overrun, and explicit WARMING/INCONCLUSIVE status. If “never exceed, including first use” is mandatory, the only valid alternative is no preview-only scene rendering until the user runs a calibration; cameras without visible faces would show EMPTY.

2. **A monitor cannot be a literal screenshot of a world face and remain cheap/deterministic.** World depth occlusion, transparency order, bloom, and final tonemapping depend on where that face appears in the main view; an offscreen face has no such pixels. The proposed monitor is exact for the feed's signal chain through mapping and TV effects. Alternative: an opt-in “screen proof” capture of visible face pixels, which would be view-dependent, fail offscreen, and violate the simple budget model.

3. **Surface Lens currently owns exactly one immutable aperture display.** `displaySelectionStatus()` rejects extra displays and removal requires removing the capture (`llprismlens.cpp:1836-1845`, `4472-4477`); mapping math is Camera-Feed-only (`llprismlens.cpp:7779-7816`). P0 should disable drag/Send for Surface Lens tiles with an explanatory tooltip. A later engine project can split “aperture” from ordinary outputs and define Surface Lens mapping; silently relaxing the invariant in this UI work is unsafe.

4. **Drag cargo is viewer-local.** It cannot cross viewer instances, inventory, or login sessions. Persisted bindings remain Director scene data, not prim state. This is intentional and must be stated in the tooltip.

5. **Exact effect reuse requires shader refactoring.** Copy/pasting `prismLensF.glsl` into a preview shader will drift and is not acceptable. The shared helper must be pixel-compared against the old world path with all-zero settings and representative effect presets before the old body is removed.

6. **Atlas/context lifetime is a crash risk if raw GL names escape.** Generation-checked render-thread resolution and the existing render-target release hook are mandatory. No tile may cache an `LLRenderTarget*`.

7. **Scene-snapshot undo can invalidate retained outputs.** Undo/redo is action-driven, not per-frame, and may cause captures to republish. Coalesce slider gestures and keep the history bounded. Do not run `applySceneData()` for hover previews.

8. **Permissions are currently looser.** Existing selected-face binding is local and does not require modify rights. Adding the requested ownership policy changes current behavior, so both manager and inline paths must move to the common validator in the same delivery.

## 8. Work packages after Director Kit Phase 0

These are review packages, not separately shippable partial features. Complete all applicable WPs, run the mandated adversarial review loop to zero must-fix, then build once and perform in-world verification.

### WP1 — Registry API, labels, signals, and history

Files: `llprismlens.h/.cpp`, `app_settings/settings.xml`, new `alprismundohistory.*`, tests.

Acceptance:

- Optional labels round-trip old/new scenes and are sanitized/bounded.
- Configuration/runtime/performance signals fire once per logical change on the main thread.
- Target-based bind/reassign preserves display settings and is atomic at the 16-face cap.
- Selection and drag validators return identical reasons.
- Undo/redo restores Add/Remove/Bind/Reassign without stale selections or partial state.

### WP2 — Preview demand in the existing scheduler

Files: `llprismlens.h/.cpp`, new `llprismpreview.h/.cpp`, `pipeline.h/.cpp`, scheduler tests.

Acceptance:

- Visible captures incur zero preview-only scene attempts.
- An unshown capture renders at preview logical size through the existing auxiliary path and global/adaptive budget.
- At most one total auxiliary attempt occurs per frame; preview-only captures make round-robin progress and never catch up in bursts.
- Previews off/hidden produces zero preview-triggered attempts while visible world faces continue.
- Context loss/release leaves no stale handles.

### WP3 — Exact signal compositor and atlas

Files: `prismLensF.glsl`, new shared shader helper and preview shaders, `llviewershadermgr.*`, `llprismlens.cpp`, `llprismpreview.cpp`, `pipeline.cpp`.

Acceptance:

- Pixel comparison of the refactored world shader before/after is exact for default settings.
- Raw preview shows capture optics and no display settings.
- Face preview matches Fit/Fill/Stretch, anchor, bars, flips/rotation, and all presets/TV controls from the same publication.
- Offline/unknown faces show OFFLINE rather than a guessed result.
- Atlas allocates once and updates at most one cell per frame with no pixel readback.

### WP4 — Budget controller and diagnostics

Files: `llprismpreview.*`, settings, test/diagnostic registration.

Acceptance:

- Non-blocking GPU timing covers both preview-only auxiliary and atlas work.
- Synthetic-cost tests prove token/admission, throttle, recovery, and no catch-up.
- Eight-capture 1 fps hook prints PASS/FAIL/INCONCLUSIVE and all stated metrics.
- After warm-up, measured rolling preview average does not exceed 0.300 ms/frame; if it cannot sustain 1 fps, effective fps drops and the UI says THROTTLED.
- Tracy shows the named preview zones.

### WP5 — Drag/drop and Send

Files: `indra/llui/llui.h`, `lltooldraganddrop.h/.cpp`, `alvcammonitorstrip.*`, `alvcampagemodel.*`, tests where practical.

Acceptance:

- VCam UUID cargo traverses LLUI and 3D paths without inventory lookup.
- Hover/drop use the same face, permission, feedback, capacity, and geometry checks as Add selected face.
- Drop on a new face binds; drop on an existing face row/world face reassigns without losing mapping/effects.
- Reassignment works at 16/16; a seventeenth new face does not.
- Failed/no-op drops create no undo entry; successful drops undo/redo as one action.
- Surface Lens drag is disabled with the documented invariant.

### WP6 — Shared inline page

Files: new `alpaneldirectorvcam.*`, `alvcampagemodel.*`, `alvcammonitorstrip.*`, new `panel_director_vcam.xml`, `llfloaterdirector.h/.cpp`, `floater_director.xml`, CMake/source lists.

Acceptance:

- UI matches the approved hierarchy: master, one-row monitors, separate camera selector/Add/Remove, Source -> Lens -> Look -> Output -> Rate & performance.
- 1-4 use large tiles and 5-8 auto-compact; monitors never select the editor camera.
- Every row binds to the scope/API in section 6.3 and global cards are visibly labeled.
- Widgets are built once; hidden tabs do no refresh work; reveal is current.
- Page and tile draw paths pass `ui_draw_lint.py`; XUI passes `xui_layout_check.py` and uses `ALScrollFit`.
- No per-frame widget lookup, setting lookup, list rebuild, string formatting, or raw widget write occurs.

### WP7 — Manager convergence and end-to-end verification

Files: `llfloaterprismmanager.h/.cpp`, `floater_prism_manager.xml` only as needed to consume shared model/signals; docs/tests.

Acceptance:

- Full manager and inline page make the same mutations and show the same rejection reasons.
- Manager's draw-driven 0.25-second snapshot poll is removed or reduced to revision-only/diff-guarded behavior consistent with `CLAUDE.md`.
- Test matrix covers master off, previews off, 0/1/4/5/8 captures, 0/16 faces, visible/offscreen/offline/virtual displays, Gate routing, Bone POV, OTS add/replace, context loss, capture removal during drag, and undo/redo.
- Adversarial review explicitly attacks main-view protection, feature-off inertness, GL state restoration, context loss, and shader default-path equivalence.
- After zero must-fix, perform one build. In-world checks verify drag face picking/permissions, monitor/world pixel comparisons, adaptive throttling, and the self-test verdict.

## 9. Definition of done

The feature is done when the Director VCam page is sufficient for normal capture creation, monitoring, face assignment, mapping/effects, and performance control without opening the manager; the manager remains an advanced fallback. With Previews off there is no preview-triggered render or atlas work. With Previews on, visible feeds are reused, preview-only feeds remain inside existing Prism/adaptive scheduling, and measured preview work obeys the declared budget contract. All mutations are revision-driven, undoable where user-visible, and the page performs no model work from `draw()`.
