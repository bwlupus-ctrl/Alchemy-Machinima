# Director's Camera / VCam Monitors Design

Status: implementation design for work after Director Kit Phase 0  
Branch reviewed: `feature/director-kit-p0`  
UI reference: approved `VCamPage.dc.html` mockup  
Scope of this document: the inline Director **Camera > VCam** page, its preview engine contract, and drag-to-surface. The existing full Virtual Cam Manager remains available as an advanced/debug surface. The v1 Lens > VCam summary remains frozen; v2 relocates VCam under Camera.

## 1. Decision summary

Build Camera > VCam as a full Director **kit page** in `floater_director_v2.xml`. Its root logic class is `ALPanelDirectorVCam`; inside it, `kit_page` owns kit cards/rows and a `monitor_strip` custom row with eight fixed `ALVCamMonitorTile` children. Source / Lens / Look / Output / Rate & performance remain in signal-chain order. The page does not edit or embed the frozen v1 `floater_director.xml`, recreate widgets, or query controls by name after `postBuild()`.

Add one engine-side `LLPrismPreviewService` beside the existing Prism registry. It does **not** introduce a second scene renderer:

- A capture already rendering for at least one visible display reuses its retained `gPipeline.mPrismLensOutput[slot]`; no extra scene render is admitted for its monitor.
- A capture with no visible display may become preview-demanded. Its next output is rendered through the existing `renderAuxiliaryView()` path, existing adaptive controller, and existing `chooseRenderSlot()` arbitration, but at a preview-only logical size and at no more than the shared preview rate.
- The existing scheduler still admits at most one auxiliary capture attempt in a presented frame. Preview-only captures participate in its round-robin/fairness cursor rather than owning another timer or render loop.
- A fixed eight-cell RGBA8 preview atlas is updated from retained capture output. One cell update applies the same signal mapping and effect math used on an in-world display. The UI only samples this atlas; it never samples a live render target whose lifetime can change under it.
- The Previews master switch removes all preview demand and schedules no atlas updates. In-world displays continue at their own picture rates. Existing retained atlas pixels may be drawn dimmed with a PAUSED badge, but they are not refreshed.

There is exactly one persisted preview master (`PrismMonitorPreviewsEnabled`) and one shared preview speed (`PrismMonitorPreviewFPS`) for all eight tiles. The page has two independent notions of choice:

1. `mSelectedCapture` chooses which camera the editor changes.
2. `mMonitorDisplay[capture-id]` chooses what that capture's monitor shows: raw signal or one of its display bindings.

Clicking a monitor never changes the edited camera. A monitor is an output view and drag source, matching the approved mockup.

There is no camera map. Camera choice is the selector plus the fixed monitor strip, with a hard cap of eight captures.

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
- Open/close one `ALKitHistory` typed transaction around a user gesture. Slider mouse-down/up is one transaction; drag/drop, Add, Remove, preset, or reset is one transaction. The model owns no history stack and no key handler.
- Preserve the existing validation and status strings by calling Prism registry APIs, never duplicating their rules in the panel.
- Emit `changed(DirtyBits)` after coalescing engine signals. The panel performs widget writes only from this callback/idle work item and only while visible.

This model is not a second source of truth. Handles and snapshots remain owned by `LLPrismLens`.

Prism settings represented by a typed transaction are excluded from the kit page's ordinary watched-setting set for that gesture. Global pure-setting rows may use normal `ALKitSettingBinding` history; no mutation is recorded by both paths.

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

using VisibilityOwner = U64;
void setConsumerVisible(VisibilityOwner owner, bool visible);
void setMonitorSelection(U32 capture_slot, const DisplayHandle&); // null = raw
AtlasHandle atlasHandle();
bool bindAtlas(const AtlasHandle&); // resolves/binds only on the render/UI thread
TileState tileState(const CaptureHandle&);
U64 revision();
boost::signals2::connection connectChanged(...);
}
```

Visibility is owner-token/refcounted: inline VCam and the advanced manager each hold a token. Preview demand is visible when at least one live owner says visible; destroying an owner removes only its claim. There is no last-writer-wins `setPanelVisible(bool)` shared by two surfaces.

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

At the start of `renderAuxiliaryView()`, after the adaptive update and before `prepare()` (`llprismlens.cpp:7174-7219`), the service publishes **preview intent** for every occupied capture only when all of these are true:

- `PrismLensEnabled` is true.
- `PrismMonitorPreviewsEnabled` is true.
- the inline panel or full manager monitor strip is in the visible chain;
- the shared preview deadline is due;
- the capture exists. Visibility is not decided here: `prepare()` resets and recomputes `mAnyDisplayVisible` (`llprismlens.cpp:2781-2805`).

Refactor `prepareCameraCapture()` (`llprismlens.cpp:2774-2921`) so the source/health/FOV checks remain common, then choose target demand:

1. visible display/Gate demand: use the existing required size and existing capture rate, and clear `mPreviewOnly`;
2. if there is preview intent and no visible/Gate demand after display preparation: render one fixed 192 x 108 logical/physical preview bucket, preserve aspect with letterbox UVs, and mark `mPreviewOnly`;
3. no demand: return false exactly as today.

`updateCadenceEntitlements()` continues to own the single global attempt budget, but uses two tiers. First it performs the existing bounded max-min water fill over all visible/Gate producers. Only the remaining capacity is water-filled among preview-only captures. Low-rate previews can never reduce a visible producer's entitlement; the current low-demand-first loop is at `llprismlens.cpp:3096-3122` and must not receive mixed visible/preview inputs. For a preview-only capture its second-tier demand is:

```text
min(shared preview FPS, capture Target FPS when Target FPS is active)
```

Automatic mode uses the shared preview FPS. Preview demand remains subordinate to the same total capacity and adaptive cadence factor; it is not a free side channel. The runtime snapshot exposes preview requested/entitled/observed values so the UI can say that 1 fps was requested but, for example, 0.5 fps was admitted.

`chooseRenderSlot()` remains the only admission point and therefore the existing “one auxiliary attempt per frame” invariant remains true. Visible/Gate due captures are considered first. Preview-only captures are considered only if none is admitted and always require their independent `mNextPreviewDueTime`, including manual Every Frame mode; they never take the unconditional automatic-producer break at `llprismlens.cpp:3185-3197`. Preview captures use a separate round-robin cursor and overdue ordering without catch-up. Do not call `renderAuxiliaryView()` a second time and do not add a panel timer that renders.

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

The cell update runs from `renderAuxiliaryView()` **after** the scheduled auxiliary scene-render scope has ended, at most once per frame. It opens its own `ScopedPrismRenderState` plus explicit scoped guards for render-target/FBO stack, `gGLViewport`, scissor enable/box, blend enable/factors, depth test/write, colour mask, bound shader, active/bound texture unit 0, camera id, and pipeline target pack. It explicitly binds/flushes the atlas target and restores every saved value before returning. With previews disabled or no queued cell, this branch returns before any GL mutation. VC3 tests both the beauty pass after an update and the feature-off path, as required by `CLAUDE.md`.

The retained source is HDR RGBA16F (`pipeline.cpp:1840-1855`). `renderAuxiliaryView()` runs before the main view (`llviewerdisplay.cpp:937-939`), so “display-referred RGBA8” uses the **previous presented frame's** completed `mExposureMap`, together with the current frame's active tonemap type/parameters, saturation/contrast grade, and linear-to-sRGB encoding. It does not claim same-frame main-view metering and does not run independent preview metering. Factor `colorCorrect`'s legacy/no-post/color-grade/tonemap/type-8 permutation choice and tonemap-uniform upload into one shared helper used by the main pass and preview compositor; do not duplicate the eight-way selection at `pipeline.cpp:12833-12888`. Preview composition explicitly excludes the main view's bloom pyramid, which is bound separately in `colorCorrect` at `pipeline.cpp:12886-12907`; capture optics/effects already present in the retained HDR signal remain. Atlas storage is sRGB-aware RGBA8 and LLUI samples it as display sRGB. Self-test reference colors cover neutral grey, highlights, at least two tonemap operators, a forced exposure change proving one-presented-frame latency, and bloom-on parity with bloom excluded from the preview transform.

“Exactly that face's result” means exact signal-space mapping, bars, flips/rotation, capture Look, and TV-effect shader math from the same retained publication. It does not mean a screenshot of the face after world depth occlusion, transparency, bloom, or final main-view tonemapping; see Risks.

### 4.4 UI drawing

`ALVCamMonitorStrip` creates eight `ALVCamMonitorTile` children in `postBuild()` and only changes visibility, rectangle, label, atlas UV, and status. Layout is one row:

- 1-4 captures: four equal large slots; unused slots hidden.
- 5-8 captures: eight equal compact slots.
- Optional explicit Large mode pages 1-4 / 5-8, as in the mockup, changes geometry only and does not rebuild children.

`ALVCamMonitorTile::draw()` contains `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam monitor tile")`, validates the atlas generation, binds it through a small scoped LLUI texture adapter, and draws one textured quad plus cached text/icons. It performs no settings reads, UUID formatting, by-name child lookup, vector/string construction, or allocation. Labels/status strings are prepared by the model on revision callbacks. The page root `draw()` contains `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Director VCam page")` and then calls `LLPanel::draw()` only.

Add a render zone `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview atlas composite")` around the atlas cell update. Preview-only scene renders nest `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism preview-only auxiliary update")` inside the existing `"Prism one scheduled auxiliary update"` zone.

### 4.5 Performance budget and self-test

The default requirement is: with eight captures requesting 1 fps, preview-triggered work contributes no more than 0.3 ms to the average presented frame **and does not fail the p95 frame-delta gate below**. Charge each preview-only auxiliary attempt at `max(render-thread CPU wall time, GPU elapsed time)`, from immediately before scratch acquisition through state restoration, so cull/stateSort/draw submission, spot-shadow work, driver stalls, and scratch-pool work are included. Charge an atlas update the same way. Ordinary renders required by visible world displays are excluded because they exist with previews off, but atlas work caused by the monitor is included.

All preview-only scenes use one dedicated 192x108 `mPrismPreviewScratch` target pack owned by `LLPipeline`, **outside** `mPrismLensScratchPool`. New `allocatePrismPreviewScratch()`, `bindPrismPreviewScratch()` and `releasePrismPreviewScratch()` APIs never enter `acquirePrismLensScratch()` and are therefore not candidates for the eight-entry LRU's eviction loop (`pipeline.cpp:1728-1825`). Allocate/validate this pinned pack when the first visibility owner appears, before demand is published; its cold allocation is separately timed and the service remains WARMING until complete. Context loss and `releasePrismLensBuffers()` explicitly release it, and the next owner rewarms it before publishing demand. Preview work never requests an aspect-specific size. Any post-warm allocation/generation change is a scratch reacquisition and fails the zero-reacquisition gate; visible captures retain all eight ordinary pool entries.

The preview quality profile is concrete and scoped. Add `LLPipeline::sPrismPreviewRender` plus `ScopedPrismPreviewQuality`. Construct that scope in `LLPrismLens::renderAuxiliaryView()` only after `chooseRenderSlot()` identifies a preview-only request and before scratch binding/camera setup (the present render scope begins at `llprismlens.cpp:7240-7259`); destroy it before `ScopedPrismRenderState` restores the main renderer. The constructor snapshots and then overrides these render variables for the scope: `RenderShadowDetail=0`, `RenderDeferredSSAO=false`, `RenderVolumetricLighting=false`, `BDMergeProjectorVolumetrics=false`, and `RenderFarClip=min(saved,64.f)`. It also caps the local lens camera/projection far plane to `min(capture far, main far, 64 m)` at the current camera-feed and surface-lens far-plane sites (`llprismlens.cpp:7322-7345`, `7378-7430`). The destructor restores every saved value in reverse order on every early return. It does **not** write `gSavedSettings`, reload shaders, change `sReflectionProbesEnabled`, or alter the retained capture's creative settings.

Because the pinned pack's deferred-light target is reused, disabling the SSAO dispatch must not leave the soften/deferred-light permutation sampling an unwritten or previous-preview AO attachment. Under `sPrismPreviewRender`, select the existing non-SSAO permutation when available; otherwise bind the renderer's neutral white AO texture at the normal AO sampler before deferred lighting. The scoped state restores the prior sampler/texture binding on every exit. VC2 clears the preview AO attachment to a non-neutral sentinel before one render and proves the result matches neutral AO, so stale AO cannot leak between captures.

Every expensive call site also checks `!sPrismPreviewRender`, so cached controls/permutations cannot bypass the profile: auxiliary sun-cascade/spot-shadow work at `llprismlens.cpp:7504-7519` and the following spot-generation block; deferred SSAO/light-map dispatch and blur at `pipeline.cpp:25523-25579` (the pack-completeness predicate is `:1752-1759`); directional volumetrics at `pipeline.cpp:18933-18961`; projector volumetrics at `pipeline.cpp:20896-20901`; and a safety early-out at `LLReflectionMapManager::update()` (`llreflectionmapmanager.cpp:260`) if it is ever invoked inside the preview scope. The normal once-per-presented-frame probe update remains later at `llviewerdisplay.cpp:1200-1206`, after the preview scope has restored the flag, so preview work neither adds a probe update nor suppresses the main view's update. Probe sampling stays enabled so material appearance does not change merely because updates are frozen. Nonessential far geometry is excluded by the scoped 64 m camera/projection limit, not by mutating the user's draw-distance setting. For `sPrismPreviewRender==false`, each predicate reduces to its current expression and no saved global is changed; VC2's review diff and feature-off test prove the visible-capture and main-view paths byte-identical.

Add a `PrismPreviewBudgetController` inside the preview service:

- Non-blocking `GL_TIMESTAMP` start/end query rings (`glQueryCounter`) measure preview-only auxiliary GPU time and atlas-composite GPU time. Do not use `GL_TIME_ELAPSED`: shader profiling already opens that target at `llglslshader.cpp:261-292`, and elapsed queries cannot nest. Results are consumed only when available; no render-thread wait.
- `LLTimer` wall intervals and Tracy CPU zones cover scratch acquisition, cull, stateSort, draw submission, atlas work, and state restoration. Matching Tracy GPU zones cover both GPU spans. Record CPU and GPU per operation separately and charge `max(cpu,gpu)`.
- The rolling allowance is `0.3 ms * presented_frames` over a 2-second window. Before admission, reserve the operation's conservative predicted charged cost; completion replaces the reservation with measured `max(cpu,gpu)`. No operation is admitted when its reservation would exceed available tokens.
- Prediction is `max(cold_seed, rolling p95 CPU, rolling p95 GPU, largest recent charged sample)`. Seeds are 1.5 ms for a 192x108 auxiliary render and 0.05 ms for an atlas update. An overrun raises prediction immediately; recovery requires 120 clean samples and decays by at most 5% per window.
- The controller is subordinate to the existing global Prism attempt budget and adaptive protection. Both must admit the work.
- If budget is exhausted, preview-only renders and atlas updates are deferred without catch-up. The requested speed is a ceiling, not a promise. UI state becomes THROTTLED and shows effective preview fps.
- Hard steady-state admission limits are charged average <= 0.300 ms/presented frame, p95 presented-frame delta <= 2.0 ms, and no charged preview operation > 4.0 ms. Crossing either tail limit immediately drops shared preview speed one step (5->2->1->0.5), then suspends preview-only renders if 0.5 still fails. These limits bound total preview cost, not just shader time.
- If an atlas draw alone cannot fit, cells retain their last image. If the condition persists, disable effects animation first only if the UI explicitly says “held”; never switch to different effect math while claiming an exact live preview.

Cold scratch allocation/calibration is not called PASS: the UI reports WARMING and admits at most one 0.5 fps preview until it has eight samples. A GPU pass cannot be pre-empted, so an unexpected sample can exceed its reservation; that sample is still charged, triggers tail throttling/suspension, and fails the self-test if it violates the hard limit. The release claim is therefore measured and enforced on the target hardware, never inferred from GPU-only timestamps.

Add non-persistent debug setting `PrismPreviewSelfTest` and console hook `LLPrismPreview::runSelfTest(seconds = 20)`. It requires the page visible, previews on at 1 fps, and eight valid captures; otherwise it logs `INCONCLUSIVE` with the missing condition. The hook runs three phases without changing user settings: 5 seconds with preview demand internally suppressed (baseline), fixed-bucket allocation/calibration until eight valid samples (WARMING), then at least 20 seconds enabled. It records presented-frame CPU wall time in both baseline and enabled phases; `p95_frame_delta_ms = p95(enabled frame ms) - p95(baseline frame ms)`. Any material camera/world/settings change restarts the baseline or makes the result INCONCLUSIVE. It records:

- presented frame count and wall duration;
- requested/admitted/produced preview-only renders per capture;
- reused-publication atlas updates;
- CPU sum/p95 and GPU sum/p95 separately for preview-only auxiliary and atlas work, including scratch acquire misses;
- charged sum (`sum(max(cpu,gpu))`), average charged ms per presented frame, p95 presented-frame delta against the rolling no-preview baseline, and maximum charged operation;
- timestamp-query coverage, Tracy-zone sample coverage, and any disjoint/unavailable interval;
- budget deferrals and achieved per-capture fps.

Verdict is `PASS` only with at least 95% CPU and GPU coverage, every capture serviced at least once, CPU-only average <= 0.300 ms/frame, GPU-only average <= 0.300 ms/frame, charged average <= 0.300 ms/frame, p95 frame delta <= 2.0 ms, maximum CPU or GPU operation <= 4.0 ms, zero scratch reacquisitions after warm-up, and no visible-display entitlement reduction. It is `FAIL` if either CPU or GPU violates its component bound (or any combined/tail bound), and `INCONCLUSIVE` for missing/unreliable timing. Log one machine-readable line:

```text
[VCamPreviewSelfTest] PASS avg_charged_ms_per_frame=0.214 cpu_aux_p95_ms=1.31 gpu_aux_p95_ms=1.72 frame_delta_p95_ms=1.78 max_op_ms=2.04 scratch_reacquire=0 frames=1204 cpu_coverage=1.00 gpu_coverage=0.99 deferred=7
```

A scheduler unit test injects independent CPU/GPU costs and scratch misses and proves: visible-first entitlement, one total auxiliary attempt per frame, preview deadlines even in Every Frame mode, round-robin progress, no catch-up, no demand while disabled/hidden, reservations never exceed tokens, and tail violations reduce rate/suspend. The in-viewer hook asserts the CPU, GPU, scratch, average, and p95 bounds.

## 5. Drag-to-surface and Send to surface

### 5.1 Cargo and 3D dispatch

Append `DAD_VCAM_FEED = 22` and increment `DAD_COUNT` in `indra/llui/llui.h:56-82`. Never renumber existing cargo values. Beside the enum/dictionary add a comment reserving 22 for viewer-local VCam cargo and `static_assert(DAD_VCAM_FEED + 1 == DAD_COUNT, "update VCam cargo value during upstream DAD re-port")`; `llinventorypanel.h:493` is currently the only fixed array sized by `DAD_COUNT`. Register it in `LLDragAndDropDictionary` (`lltooldraganddrop.cpp:211-236`) with only `DT_OBJECT -> dad3dVCamFeed`; self, avatar, land, and none reject.

Add `LLToolDragAndDrop::ESource::SOURCE_VCAM` after `SOURCE_VIEWER` (`lltooldraganddrop.h:61-68`). It is a UUID-only source: `locateInventory()` returns null immediately for `SOURCE_VCAM`, and `is_uuid_dragged = (mSource == SOURCE_PEOPLE || mSource == SOURCE_VCAM)`. Do not use `SOURCE_VIEWER`: its inventory lookup returns the toolbar's cached drag item (`lltooldraganddrop.cpp:3371-3374`), while `resetDragTool` does not clear `mDragItem` (`lltoolbarview.cpp:949-965`).

`ALVCamMonitorTile::handleMouseDown/hover` starts:

```cpp
LLToolDragAndDrop::instance().beginDrag(
    DAD_VCAM_FEED, capture.mId, LLToolDragAndDrop::SOURCE_VCAM);
```

Teach both top/root UI traversal and 3D dispatch in `LLToolDragAndDrop::dragOrDrop()` to forward a UUID pointer for `SOURCE_VCAM && DAD_VCAM_FEED`, analogous to person UUID cargo. The top-view second pass must pass the actual `drop=true`; the current people branch incorrectly passes false at `lltooldraganddrop.cpp:573-595`, and the new source may not copy that bug. No inventory lookup is performed. Registry UUID lookup returns the current generation-bearing handle; removal during a drag therefore becomes a stale-cargo rejection.

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

Dropping on an unbound face creates a display with the page's “new face mapping” default. Dropping on a face already represented by a Prism display atomically changes its capture owner, preserving `DisplayHandle`, mapping, virtual/real identity, Gate subscription, and TV effects. It decrements/increments display counts, drops the old capture's publication demand when it has no display, Gate, or preview demand, bumps configuration revision once, and emits one change signal. It does **not** release the retained output: outputs are lifetime-fixed once allocated (`pipeline.cpp:1828-1855`) and release only on capture/resource teardown.

Scene restore uses the same target validator with `ERestorePolicy::SCENE`, but permission is backward-compatible. Parse and structurally validate the complete Prism payload first. Malformed identities/schema, impossible capacity, feedback cycles, or invalid live geometry still reject the Prism replacement atomically. A live face whose **only** failure is `permModify()` does not reject the scene: omit/disable that display binding, retain its serialized identity and settings in a quarantined record with reason “Skipped on scene load: no modify permission”, and continue applying every capture and every other valid display. An offline object is retained quarantined/offline; when it resolves, the validator runs before publication and a permission failure leaves only that binding disabled with the same visible reason.

`applySceneData` returns an `ApplyReport` (success, fatal reason, and skipped-binding records) rather than collapsing warnings into failure. `LLFloaterDirector::loadScene` keeps its existing all-or-nothing early return for a fatal Prism result before it mutates cast/settings (`llfloaterdirector.cpp:1478-1494`), but a successful report with permission skips continues through the Director apply (`:1520-1664`) and posts one summary: “Scene loaded; N monitor face(s) skipped for permission”, with object/face details in the log and VCam rows. Thus existing scenes remain loadable, unauthorized live faces are never published, and only permission failures are partial; structural corruption remains atomic.

Dropping a VCam tile on a face row calls the same `reassignDisplay(display, capture)` API. The row is an LLUI drop target; it does not invoke a world pick.

### 5.3 Send to surface

The tile menu's **Send to surface...** opens a small anchored menu with:

- **Bind selected prim face**: calls `bindSelectedFace(capture, selected new-fit mode)` immediately, using the current exactly-one-face selection.
- **Move existing face...**: submenu of display bindings owned by other captures; calls `reassignDisplay`.
- **Cancel**.

If there is no eligible selected face, keep the command visible but disabled with the exact `displayTargetStatus` reason. Do not enter a modal “waiting for selection” mode; selection can change independently and the page refreshes its enablement from the selection signal.

### 5.4 Undo

VCam uses the Director-wide `ALKitHistory`; there is no `ALPrismUndoHistory`, page-local stack, or second Ctrl+Z owner. `ALVCamPageModel::transaction()` records typed `ALKitHistory::CustomEntry` operations with stable handles and a registry scene-generation context. The VCam kit bar uses the normal page Undo/Redo buttons, and Director keyboard rules remain Ctrl+Z/Ctrl+Y; Ctrl+Shift+Z is untouched.

Full-scene snapshot replay is forbidden. Undo/redo never calls `applySceneData()`: its commit path releases every output and scratch pack, resets Gate, reconstructs captures/displays with new generations, and resets runtime history (`llprismlens.cpp:5503-5579`). Fine-grained operations are:

- `setCamera`, `setRate`, `setDisplay`, `setLabel`, and Gate-subscription changes store exact before/after values and call the matching setter on the same handle;
- bind/unbind/reassign stores capture/display handles, target object+TE, and display settings, and calls inverse `unbindDisplay`, `bindDisplayTarget`, or `reassignDisplay` without reallocating unrelated objects;
- Add undo removes only the created stable handle (and only that capture's output if allocated); redo calls new `restoreCaptureIdentity(const RemovedCaptureRecord&)` / `restoreDisplayIdentity(...)` APIs that reuse the reserved UUID **and generation**;
- Remove first obtains an immutable `RemovedCaptureRecord` containing the capture, owned displays and stable handles/generations, plus the complete Gate inverse **before** `clearSlot`: the optional `GateArmedCamera` (arm UUID, label, enabled flag and capture UUID), original arm index, whether it was Program, Preview, Warm and/or On Air, and every gate-subscribed display handle that removal will detach. This is required because `clearSlot` detaches subscribed displays at `llprismlens.cpp:6031-6037` and disarms the capture with full index/preview/on-air fixup at `:6045-6057`.
- Undo calls new atomic `restoreCaptureIdentity(record)` followed inside the same registry transaction by `gateRearm(record)`. `gateRearm` inserts the same arm identity at its recorded index (shifting later indices), reattaches the recorded subscribed displays to the restored capture generation, restores Program/Preview/Warm indices by arm identity, and restores On Air only when the record says that arm was on air; otherwise it does not TAKE or punch the restored camera to program. If any identity, capacity, feedback or display-reattachment precondition fails, it rolls back the whole inverse and leaves the history step intact. Redo removes those same handles and repeats the normal detach/disarm behavior. Only the restored/removed capture may republish; unrelated outputs, the ordinary and pinned scratch packs, Gate arms/routing and cut serial remain unchanged.
- OTS pair uses one compound typed entry and identity-preserving restore for both captures/arms; preset/reset records affected typed settings, never a scene blob;
- a slider gesture coalesces repeated setter calls into one before/after typed entry.

The registry reserves identities referenced by undo steps; capacity UI counts live objects only, while a bounded reservation table is capped by `ALKitHistory` eviction. When a step is evicted/barriered, its reservations release. A restore precondition failure leaves the step intact and shows “Cannot undo <label>: <reason>”; it never partially restores. Director scene load, registry wholesale replacement, and logout call the history barrier.

Drag/drop, Send, Add, Remove, preset/reset, and completed slider gestures use this path. Selection is restored by stable handle only after the inverse succeeds. No server object-edit undo entry is added because no simulator object was edited. Failed/no-op actions create no entry. Tests remove/undo/redo an unarmed capture and arms at the first/middle/last index while each is Program, Preview, Warm, On Air or off air; they verify arm UUID/label/enabled/index, subscribed display handles, Program/Preview/Warm/On-Air routing, unrelated outputs, and cut serial. Gate routing changes only when it is part of the removed capture's recorded inverse.

## 6. Inline page and model binding

### 6.1 Files and layout

Add:

- `alpaneldirectorvcam.h/.cpp` — page controller and cached widget pointers.
- `alvcampagemodel.h/.cpp` — selection, commands, snapshots, dirty coalescing, undo integration.
- `alvcammonitorstrip.h/.cpp` — fixed monitor layout and tile LLViews.
- generated `skins/default/xui/en/panel_dpage_camera_vcam.xml` — Director kit page.

The generated root is `<panel class="panel_director_vcam" name="vcam_tab_logic">` containing `<kit_page page="camera_group/vcam_tab" ...>`. Its first full-span card contains one `<kit_row kind="monitor_strip"><vcam_monitor_strip .../></kit_row>`; subsequent kit cards/rows are Source, Lens, Look, Output, and Rate & performance in that order. Capture/display/global rows bind through `ALKitBinding`/`ALKitWidgetBinding` and `ALVCamKitAdapter`. The kit page owns the one scroll document; no nested page scroller or hand-authored document height exists.

The v1 `floater_director.xml:1342-1353` remains byte-identical. The full manager shares `ALVCamPageModel` commands/signals incrementally, not the page view instance. VCam uses the normal kit page bar/search/help/amber/chips/history, and the monitor strip enumerates audit states for 0/1/4/5/8 captures and every selector option.

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

## 7. Feasibility boundaries and risk controls

1. **The budget is a measured admission contract, not a shader estimate.** Cold allocation/calibration is WARMING and rate-limited; the shipped PASS claim begins only with reliable CPU+GPU samples. Reservation, fixed scratch size, quality profile, rate reduction, and suspension enforce the average/tail gates in section 4.5. A submitted GPU pass cannot be pre-empted, so an unexpected overrun is charged and fails the gate rather than being hidden.

2. **A monitor cannot be a literal screenshot of a world face and remain cheap/deterministic.** World depth occlusion, transparency order, bloom, and final tonemapping depend on where that face appears in the main view; an offscreen face has no such pixels. The proposed monitor is exact for the feed's signal chain through mapping and TV effects. Alternative: an opt-in “screen proof” capture of visible face pixels, which would be view-dependent, fail offscreen, and violate the simple budget model.

3. **Surface Lens currently owns exactly one immutable aperture display.** `displaySelectionStatus()` rejects extra displays and removal requires removing the capture (`llprismlens.cpp:1836-1845`, `4472-4477`); mapping math is Camera-Feed-only (`llprismlens.cpp:7779-7816`). P0 should disable drag/Send for Surface Lens tiles with an explanatory tooltip. A later engine project can split “aperture” from ordinary outputs and define Surface Lens mapping; silently relaxing the invariant in this UI work is unsafe.

4. **Drag cargo is viewer-local.** It cannot cross viewer instances, inventory, or login sessions. Persisted bindings remain Director scene data, not prim state. This is intentional and must be stated in the tooltip.

5. **Exact effect reuse requires shader refactoring.** Copy/pasting `prismLensF.glsl` into a preview shader will drift and is not acceptable. The shared helper must be pixel-compared against the old world path with all-zero settings and representative effect presets before the old body is removed.

6. **Atlas/context lifetime is a crash risk if raw GL names escape.** Generation-checked render-thread resolution and the existing render-target release hook are mandatory. No tile may cache an `LLRenderTarget*`.

7. **Identity-preserving typed undo is mandatory.** `applySceneData()` is forbidden for undo/redo. Stable UUID+generation reservations and fine-grained inverse operations preserve unrelated outputs and Gate state; Remove alone records and exactly reverses its own arm/display detachment. Slider gestures coalesce and history barriers release reservations.

8. **Permissions are currently looser.** Existing selected-face binding is local and does not require modify rights. Adding the requested ownership policy changes current behavior, so both manager and inline paths must move to the common validator in the same delivery.

## 8. Work packages after Director Kit Phase 0

These are review packages, not separately shippable partial features. Complete VC1-VC7, run the mandated adversarial review loop to zero must-fix, then build once and perform in-world verification. In the rollout plan, VC1-VC5 are engine work and may be completed before rollout WP6. Rollout WP9 then consumes them in this order: VC6 (inline Director page) followed by VC7 (manager convergence and end-to-end verification). VCam uses only the VC prefix.

### VC1 — Registry API, labels, signals, and history

Files: `llprismlens.h/.cpp`, `app_settings/settings.xml`, `alkithistory.*`, `alvcampagemodel.*`, tests.

Acceptance:

- Optional labels round-trip old/new scenes and are sanitized/bounded.
- Configuration/runtime/performance signals fire once per logical change on the main thread.
- Target-based bind/reassign preserves display settings and is atomic at the 16-face cap.
- Selection and drag validators return identical reasons.
- Typed `ALKitHistory` undo/redo restores Add/Remove/Bind/Reassign with the same UUID+generation and no partial state. Remove restores its exact Gate arm/index/role flags and subscribed displays; only the capture explicitly removed/restored may lose/republish its output, while unrelated outputs, scratch packs and Gate state remain unchanged. No undo path calls `applySceneData()`.
- Old scenes with live faces that fail only `permModify()` load successfully, quarantine only those bindings, report each skip, and continue the Director scene apply; malformed/structurally invalid Prism data still rejects atomically before Director mutation.

### VC2 — Preview demand in the existing scheduler

Files: `llprismlens.h/.cpp`, new `llprismpreview.h/.cpp`, `pipeline.h/.cpp`, scheduler tests.

Acceptance:

- Visible captures incur zero preview-only scene attempts.
- An unshown capture renders in the dedicated pinned 192x108 preview target, outside the eight-entry visible-capture LRU, through the scoped preview-only quality profile and global/adaptive/budget admission.
- The scope pushes/restores the enumerated pipeline flags and 64 m camera limit on every early return; each shadow/SSAO/volumetric/probe-update gate sees `sPrismPreviewRender`, while its false path is byte-identical.
- With SSAO dispatch suppressed, deferred lighting uses the non-SSAO permutation or a neutral white AO binding; a sentinel-stale AO test proves no previous preview can darken the next one.
- At most one total auxiliary attempt occurs per frame; preview-only captures make round-robin progress and never catch up in bursts.
- Visible/Gate water-filling completes before previews receive leftover capacity; manual Every Frame previews still obey their preview deadline.
- Previews off/hidden produces zero preview-triggered attempts while visible world faces continue.
- Context loss/release leaves no stale handles.

### VC3 — Exact signal compositor and atlas

Files: `prismLensF.glsl`, new shared shader helper and preview shaders, `llviewershadermgr.*`, `llprismlens.cpp`, `llprismpreview.cpp`, `pipeline.cpp`.

Acceptance:

- Pixel comparison of the refactored world shader before/after is exact for default settings.
- Raw preview shows capture optics and no display settings.
- Face preview matches Fit/Fill/Stretch, anchor, bars, flips/rotation, and all presets/TV controls from the same publication.
- Offline/unknown faces show OFFLINE rather than a guessed result.
- Atlas allocates once and updates at most one cell per frame with no pixel readback.
- Atlas updates restore complete GL/FBO/pipeline state, leave the beauty pass unchanged, and the feature-off path performs no GL mutation.
- HDR-to-RGBA8 output uses the previous presented frame's exposure map plus current tonemap/grade/sRGB parameters through the shared permutation helper; the one-frame exposure latency and explicit bloom exclusion match section 4.3.

### VC4 — Budget controller and diagnostics

Files: `llprismpreview.*`, settings, test/diagnostic registration.

Acceptance:

- Non-blocking timestamp timing and CPU wall/Tracy zones cover scratch acquisition through restoration for both preview-only auxiliary and atlas work.
- Synthetic independent CPU/GPU/scratch costs prove reservation, visible-first admission, tail throttle, recovery, and no catch-up.
- Eight-capture 1 fps hook prints PASS/FAIL/INCONCLUSIVE and every CPU, GPU, charged, scratch, p95, maximum, coverage, and entitlement metric.
- After warm-up, charged average is <=0.300 ms/frame, p95 frame delta <=2.0 ms, maximum operation <=4.0 ms, and scratch reacquisition is zero; otherwise shared rate drops and the UI says THROTTLED/suspended.
- Tracy shows the named CPU and GPU preview zones.

### VC5 — Drag/drop and Send

Files: `indra/llui/llui.h`, `lltooldraganddrop.h/.cpp`, `alvcammonitorstrip.*`, `alvcampagemodel.*`, tests where practical.

Acceptance:

- `SOURCE_VCAM` UUID cargo traverses LLUI and 3D paths without inventory lookup or stale toolbar cargo; the top-view drop pass receives `drop=true`.
- Hover/drop use the same face, permission, feedback, capacity, and geometry checks as Add selected face.
- Drop on a new face binds; drop on an existing face row/world face reassigns without losing mapping/effects.
- Reassignment works at 16/16; a seventeenth new face does not.
- Failed/no-op drops create no undo entry; successful drops undo/redo as one action.
- Surface Lens drag is disabled with the documented invariant.

### VC6 — Director kit page

Files: new `alpaneldirectorvcam.*`, `alvcampagemodel.*`, `alvcammonitorstrip.*`; generated `panel_dpage_camera_vcam.xml`, generator config/data/index; `llfloaterdirector.h/.cpp` only for shared adapter wiring; CMake/source lists. `floater_director.xml` is off-limits.

Acceptance:

- The full `kit_page` matches the approved hierarchy: master, one-row monitor custom row, separate camera selector/Add/Remove, Source -> Lens -> Look -> Output -> Rate & performance.
- 1-4 use large tiles and 5-8 auto-compact; monitors never select the editor camera.
- Every row binds to the scope/API in section 6.3 and global cards are visibly labeled.
- Widgets are built once; hidden tabs do no refresh work; reveal is current.
- Search/help/amber/chips and the sole `ALKitHistory` work; no page-local history/key handler exists.
- Page and tile draw paths pass `ui_draw_lint.py`; kit XUI and all non-default states pass `xui_layout_check.py`/runtime audit through the kit-owned scroller.
- No per-frame widget lookup, setting lookup, list rebuild, string formatting, or raw widget write occurs.
- Moving VCam from the v1 Lens group to the v2 Camera group preserves name-based `selectLeafTab` routing (`llfloaterdirector.cpp:809-821`); tests cover `migrate_tab_name`, `DirectorLastTabName`, and v1→v2/v2→v1 floater rect plus selected-tab carry-over.

### VC7 — Manager convergence and end-to-end verification

Files: `llfloaterprismmanager.h/.cpp`, `floater_prism_manager.xml` only as needed to consume shared model/signals; docs/tests.

Acceptance:

- Full manager and inline page make the same mutations and show the same rejection reasons.
- Manager's draw-driven 0.25-second snapshot poll is removed or reduced to revision-only/diff-guarded behavior consistent with `CLAUDE.md`.
- Test matrix covers master off, previews off, 0/1/4/5/8 captures, 0/16 faces, visible/offscreen/offline/virtual displays, Gate routing, Bone POV, OTS add/replace, context loss, capture removal during drag, and undo/redo.
- Adversarial review explicitly attacks main-view protection, feature-off inertness, GL state restoration, context loss, and shader default-path equivalence.
- After zero must-fix, perform one build. In-world checks verify drag face picking/permissions, monitor/world pixel comparisons, adaptive throttling, and the self-test verdict.

## 9. Definition of done

The feature is done when the Director VCam page is sufficient for normal capture creation, monitoring, face assignment, mapping/effects, and performance control without opening the manager; the manager remains an advanced fallback. With Previews off there is no preview-triggered render or atlas work. With Previews on, visible feeds are reused, preview-only feeds remain inside existing Prism/adaptive scheduling, and measured preview work obeys the declared budget contract. All mutations are revision-driven, undoable where user-visible, and the page performs no model work from `draw()`.
