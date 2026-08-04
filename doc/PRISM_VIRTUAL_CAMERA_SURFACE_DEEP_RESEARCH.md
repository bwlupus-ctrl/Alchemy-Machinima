# Prism shared virtual camera feeds — deep research and implementation brief

Status: code-complete implementation candidate in the working tree; static/adversarial source review completed; no build performed

Repository snapshot reviewed: 2026-08-04

Practical limit: three capture producers and sixteen display bindings, with at most one auxiliary scene render per main frame

Implementation note (2026-08-04): this is no longer theory or a copy/paste code
example. The working tree now contains the bounded capture/display registry,
Blender-axis virtual-camera render path, one-capture-to-many-face compositor,
Fit/Fill/Stretch mapping, per-capture picture FPS, adaptive/manual performance
controls, transactional Director version-3 persistence, a full Prism Manager
floater, and fixed-capacity retained outputs. It intentionally has not been
configured, compiled, linked, or launched; the first authorized build remains
the compile/shader/runtime validation gate.

The current adaptive controller is deliberately truthful: it uses presented-FPS
feedback and reports GPU timing as unavailable. It can reduce resolution,
throttle cadence, hold the last valid frame, or suspend captures to protect the
selected 30/45/60 FPS main-view target, but it cannot guarantee 30 FPS on a scene
that already misses 30 without Prism. The nonblocking GPU timestamp/controller
model later in this document remains a recommended follow-up, not implemented
code in this patch.

Implementation truth boundary: Sections 18.2-18.4 describe the stronger
measurement/admission controller researched for a later revision; they are not
claims about the current hysteretic presented-FPS feedback guard. Section 20 is the original
target integration map and includes proposed files that were not ultimately
created. For the exact implemented file list and current validation status, use
`doc/PRISM_CAMERA_FEED_CLAUDE_HANDOFF.md`.

## 1. Correction and executive decision

This revision supersedes the earlier one-capture-per-display draft. Camera feeds
now fan one retained capture out to multiple independently mapped faces.

The requested effect is **not a mirror** and is **not another eye-through-surface
magnifier**. It is a conventional render-to-texture camera:

```text
ordinary camera-marker prim
        |
        | position + rotation + FOV
        v
auxiliary perspective scene render
        |
        v
retained linear-HDR 2D texture
        |
        +----------+----------+
        v          v          v
 display face A  face B     face C ...
```

The display face says where the picture appears. It does not determine the
camera position or look direction. A separate source object acts like a Blender
camera: move it, rotate it, and point it at the subject. The source looks along
its local `-Z` axis with local `+Y` as up. That convention also matches
Alchemy's existing projector direction, but the source does **not** have to be
a light or projector.

One deliberate first-version difference from Blender is that the source is an
ordinary SL prim, not a non-rendering camera datum. Use a transparent marker or
the supplied forward eye offset; automatic removal from cached render batches
is not claimed in this design.

This is feasible on the current Prism foundation. In fact, it is simpler than
the mirror proposal:

- no reflected eye;
- no generalized off-axis projection for camera-feed mode;
- no destination-face clip plane;
- no reflection probe, hero probe, cubemap, radiance filtering, or mirror flag;
- one ordinary symmetric perspective view rendered into the already-existing
  Prism scratch target;
- the already-existing real-face HDR compositor displays that retained image on
  every face bound to the capture.

The right product shape is to retain the current magnifier and add
`CAMERA_FEED` as a second Prism capture mode. Separate expensive capture
producers from cheap display bindings:

- at most three capture producers total (`SURFACE_LENS` or `CAMERA_FEED`);
- at most sixteen display bindings total;
- a Lens capture owns exactly one face because that face defines its optical
  aperture;
- a Camera Feed capture owns one retained texture and may drive many faces;
- one shared auxiliary-render and adaptive performance budget schedules only
  producers, never individual faces.

One camera feeding ten faces therefore performs one scene capture plus ten
small face composites—not ten scene captures.

## 2. What this behaves like

The closest established engine analogue is Unreal's `SceneCaptureComponent2D`:
it captures one planar camera view into a 2D render target. Epic explicitly
describes it as a camera feeding a TV/security-monitor-like surface, rather
than a reflection. Unity exposes the same pattern through
`Camera.targetTexture`. Blender separates camera-object transform from camera
lens properties and the output image.

That is exactly the model recommended here:

| Concern | Owner |
|---|---|
| Camera position and aim | Source object's live render transform |
| Camera forward/up convention | Local `-Z` forward, local `+Y` up |
| Vertical field of view | Per-capture fixed value, optionally inherited from a projector |
| Near/far clipping | Per-capture camera settings |
| Image aspect | Per-capture canonical output aspect, default 16:9 |
| Capture resolution | Largest visible binding demand, stable buckets, adaptive scale |
| Image placement | Per-binding Fit/Fill/Stretch through the Prism compositor |
| Picture update rate | Per-capture `Automatic` or `Target FPS`; all bound faces inherit the producer's one publication cadence |
| Total capture budget | Global admitted auxiliary-attempt budget shared fairly across at most three producers |

This is a **monitor surface**, not a projected light. It changes only the
designated face. It does not illuminate nearby objects or throw the image onto
other geometry.

The performance objective has three distinct meanings:

- one eligible producer of either mode may target a 30 Hz picture refresh when
  the main viewer runs at least 30 FPS and the configured/feedback-controlled
  budget admits the capture;
- a user may request a lower 1-30 FPS picture rate on each capture; it is a
  ceiling, not a promise, and every face bound to that capture receives the same
  retained texture revision at the same publication time;
- Adaptive mode protects a 30 FPS **main-view target** by reducing capture
  resolution/cadence or pausing capture. It cannot manufacture 30 FPS when the
  same scene is already below 30 FPS with Prism disabled.

## 3. Probe-free contract

Camera-feed implementation must never create, mutate, or require any of these:

- `LLReflectionProbeParams`;
- `LLHeroProbeManager` ownership or selection;
- `LLReflectionMap` capture state;
- cubemap arrays or cube snapshots;
- probe-object flags or simulator probe messages;
- `mHeroProbeRT`;
- `RenderMirrors`;
- `sReflectionProbesEnabled` as an availability gate.

Ordinary PBR objects visible through the feed may still use the viewer's normal
IBL reflection probes when that unrelated feature is enabled. Turning all
reflection probes and native mirrors off must not disable the camera feed.

## 4. Local implementation evidence

The current implementation already contains most of the expensive machinery:

- `llprismlens.h`: fixed `MAX_CAPTURES = 3` and
  `MAX_DISPLAY_BINDINGS = 16` caps plus public registry/composite APIs;
- `llprismlens.cpp`: display-face validation, a geometry-keyed local surface-basis
  cache with staggered 4–5 second defensive revalidation, visible screen-footprint
  calculation, retained-output state, fair scheduler, full auxiliary camera/GL
  state scope, and the one-view render;
- `pipeline.cpp`: three lazily allocated exact-size capture scratch packs,
  three retained `GL_RGBA16F` outputs, exact-size Prism water intermediates,
  remote-view containment, and depth-tested face compositing;
- `prismLensV.glsl` / `prismLensF.glsl`: canonical raw-surface UV generation and
  retained HDR sampling;
- `llviewerdisplay.cpp`: auxiliary render before the main view completes;
- `llfloaterprismmanager.cpp` / `floater_prism_manager.xml`: dedicated
  capture/display registry, camera/rate editor, and performance UI;
- `llfloaterdirector.cpp` / `floater_director.xml`: compact summary/manager
  entry point and version-3 scene persistence.

Current working-tree boundary: Camera Feed, per-capture output FPS, capture-to-
many-display fan-out, the Prism Manager floater, version-3 Director persistence,
and the bounded scheduler are implemented. They have received static/adversarial
source review, but no configure, compile, shader-link, client launch, or runtime
performance validation has been performed.

The implemented retained-output array matches the three-capture cap, while the
separate composite-state array matches the sixteen-binding cap. Expanding
display state does not allocate sixteen render targets.

The native projector path establishes a useful camera-marker convention:

- `pipeline.cpp`, `setupHWLights()`: spotlight direction is local `(0,0,-1)`
  rotated by `getRenderRotation()`;
- `pipeline.cpp`, projector/shadow setup: projector FOV is
  `getSpotLightParams()[0]` in radians;
- `llviewerobject.cpp`, `getRenderPosition()` and `getRenderRotation()`:
  render-space transforms include active/link-child/attachment handling that
  raw object-local transforms do not.

The camera source must therefore be resolved by UUID every scheduled update and
sampled through its **render** transform. Never retain an `LLViewerObject*` or
`LLDrawable*` across frames.

## 5. Complete user workflow

Source and screen selection are separate, explicit operations. Never infer both
from a multi-object selection.

Camera-feed workflow:

1. Open Director > Camera > `Manage Prism`.
2. Select exactly one ordinary world object and click `Add camera`.
3. The new capture is highlighted. Rotate its marker so local `-Z` points at the
   subject and move it to frame the shot.
4. Configure FOV, near/far clip, eye offset, and canonical output aspect
   (`16:9`, `4:3`, `1:1`, or Custom).
5. Set `Picture update rate` to `Automatic` or `Target FPS`. Target accepts a
   finite value from 1 through 30 FPS and offers 5, 10, 12, 15, 20, 23.976, 24,
   25, 29.97, and 30 presets.
6. Select exactly one rectangular world face and click `Add selected face`.
7. Repeat step 6 for every monitor that should show that camera. Each binding
   selects `Fit`, `Fill`, or `Stretch`; no new scene render is created.

Lens workflow remains face-first: select one face and click `Add lens`. That
creates one producer and its required single binding. A Lens cannot gain extra
bindings because its designated face defines the off-axis aperture. Its mapping
is forced to the legacy full-face orientation; Fit/Fill/anchor controls apply
only to Camera Feed displays. The Lens's primary binding cannot be removed on its
own: Add Display, Remove Display, Fit, Fill, Stretch, anchor, and bar controls are
disabled with an explanatory tooltip. `Remove Capture` is the only removal path
and confirms the one-binding cascade.

The main Camera tab has insufficient vertical space for two registries and
optics/performance controls. Replace its current action row with the compact
summary `Prism 2/3 captures • 5/16 displays` plus `Manage…`. A dedicated Prism
manager floater contains:

- capture list (maximum three) with Add Camera, Add Lens, Edit, Remove;
- display list for the highlighted capture with mode-aware Add Selected Face,
  Fit mode, Remove, and Locate actions. Camera Feed enables them; Lens exposes
  only Locate and the capture-level removal path;
- capture editor with `Set selected camera`, Follow Projector FOV, vertical FOV,
  near/far, eye offset, `Place eye in front`, output aspect, and `Picture update
  rate` (`Automatic` or `Target FPS`, 1-30). The tooltip states that every linked
  face uses this one texture/rate and Adaptive may deliver less;
- performance panel with `Adaptive performance` (default on), main-view target
  30/45/60 FPS (30 default), `Total capture budget` (30 attempts/s default), and
  quality ceiling (existing 0.25–2.0 range). These controls govern every auxiliary
  producer, both Lens and Camera Feed;
- global readout such as `Presented 31.8 FPS • render p95 31.4 ms • applied scale
  0.50–0.75x • admitted 20 attempts/s`; each capture row separately shows its scheduler entitlement
  and observed successful publication rate. The capture editor shows
  `Requested / Entitled / Observed`, for example `24 -> 15.0 / 14.8 FPS`;
- each Display row has a read-only value such as
  `Inherited from Camera 2 - 14.8 FPS`. A face never owns an editable rate.

Recommended rows:

```text
CAPTURES
#  TYPE    HEALTH          OUTPUT   ACTIVITY   DISPLAYS  SCALE  REQUEST/ENTITLED/OBSERVED
1  Lens    READY           CURRENT  LIVE       1         1.00x  AUTO/10.0/9.8 FPS
2  Camera  SOURCE_OFFLINE  HELD     WAITING    4         0.75x  24/0.0/0.0 FPS
3  Camera  READY           CURRENT  THROTTLED  2         0.50x  24/10.0/9.6 FPS

DISPLAYS FOR CAMERA 2
HEALTH  VISIBILITY  FACE          MAPPING   PICTURE FPS
READY   VISIBLE     12ab34cd f2   FIT       inherited 0.0
OFFLINE UNKNOWN     98ef76ab f0   FILL      inherited 0.0
READY   OFFSCREEN   44cc11aa f3   STRETCH   inherited 0.0
```

There is no combined-badge precedence. Capture Health, Output, and Activity are
independent columns, so `SOURCE_OFFLINE + HELD + WAITING` is representable
without hiding any fact. `IDLE` means a valid producer currently has no visible
consumer; `WAITING` means visible demand exists but the producer cannot yet
publish; `THROTTLED` means it is still updating below the requested
scale/cadence; and `PAUSED` means capture attempts are deliberately suspended.
Display Health and Visibility are likewise independent: `OFFLINE` is an object
or face resolution failure, while `OFFSCREEN` is a healthy face with no current
main-view demand. An unhealthy binding reports visibility `UNKNOWN`.

Full UUIDs, face indices, generations, rejection reasons, output age, and
effective performance values belong in tooltips. Runtime changes update without
notification spam.

An unbound or zero-display camera still consumes one of three capture slots but
is `IDLE` and does no auxiliary rendering. Display bindings consume the separate sixteen-face
cap. A display `(UUID, TE)` may belong to only one producer. Removing a capture
removes its bindings only after confirmation; removing one binding never removes
its capture or siblings. Changing Lens/Camera mode is explicit remove/re-add.

The existing Zoom control remains Lens-only. User quality is a ceiling;
Adaptive mode may lower the effective capture scale/cadence of either producer
mode to protect the selected main-view FPS target and restores quality slowly
when headroom returns.

### 5.1 Required Prism Manager floater and scrolling contract

Implement a dedicated `LLFloaterPrismManager` backed by
`floater_prism_manager.xml`; do not stretch the Director Camera tab into a second
registry. The floater is single-instance, saves its rectangle, and is resizable.
A practical starting rectangle is 900x680 with a 720x520 minimum. The minimum is
an acceptance constraint: every operation must remain reachable at that size.

Use one vertical layout stack:

1. fixed 72-80 px global summary/performance strip;
2. fixed 145-155 px capture list and Add Camera/Add Lens/Remove actions;
3. one elastic detail tab container;
4. fixed 24 px validation/status footer.

The Displays tab owns its `scroll_list` and bottom-anchored Add/Remove/Locate/Fit
actions. Capture and Performance each own a `scroll_container follows="all"`
whose **direct first child** is a definite-natural-height settings document. That
document follows left/top/right, never bottom or vertical `all`; otherwise it
shrinks with the viewport and the scrollbar can disappear while controls clip.
Reserve the scrollbar gutter and do not wrap the entire floater or any display
list in a competing page scroller.

The Capture document is mode-aware: its common Picture update-rate group is
always visible; Camera Feed shows source/optics/aspect controls; Surface Lens
shows Lens-only controls and hides Camera Feed fields without leaving dead space.

Selection is retained by persistent UUID plus generation. Selecting a record,
revealing a validation error, or programmatically exposing a setting calls
`LLScrollContainer::scrollToShowRect()`. In `postBuild()`, also register a
`setFocusReceivedCallback()` on every focusable control inside each document.
Convert the focused view's local rectangle to document coordinates before
scrolling; `LLScrollContainer` does not do this automatically:

```cpp
LLRect document_rect;
if (focused_view && focused_view->localRectToOtherView(
        focused_view->getLocalRect(), &document_rect, settings_document))
{
    settings_scroll->scrollToShowRect(document_rect);
}
```

This keeps keyboard Tab focus visible. Poll bounded snapshots at most 4 Hz and
update age text at most 1 Hz.
The Director Camera tab retains only master enable, compact capture/display
counts and health, and `Manage Prism...` above `camera_flow_scroll`.

Minimum class contract:

```cpp
class LLFloaterPrismManager final : public LLFloater
{
public:
    explicit LLFloaterPrismManager(const LLSD& key);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;

private:
    void refreshConfiguration();
    void refreshRuntime();
    void refreshPerformance();
    void refreshSelectionActions();
    void installDocumentFocusReveal();

    LLPrismLens::CaptureHandle mSelectedCapture;
    LLPrismLens::DisplayHandle mSelectedDisplay;
    U64 mConfigurationRevision = 0;
    U64 mRuntimeRevision = 0;
    U64 mPerformanceRevision = 0;
    LLFrameTimer mRefreshTimer;
    LLFrameTimer mAgeRefreshTimer;
};
```

Register it beside Director and open it through the normal floater registry:

```cpp
LLFloaterReg::add(
    "prism_manager",
    "floater_prism_manager.xml",
    (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPrismManager>);

// Director's Manage Prism... callback
LLFloaterReg::showInstance("prism_manager");
```

The form-scroller XUI pattern is equally normative:

```xml
<scroll_container
 follows="all"
 reserve_scroll_corner="false"
 size="15"
 width="864">
    <panel
     follows="left|top|right"
     height="430"
     name="capture_settings_document"
     width="847">
        <!-- common rate plus the selected mode's settings -->
    </panel>
</scroll_container>
```

The example's document is explicitly 17 px narrower than its initial scroller
(15 px `UIScrollbarSize` plus borders); both resize from the right so the gutter
persists. `reserve_scroll_corner="false"` controls only the lower corner and is
not a substitute for this width inset. Recalculate the paired initial widths if
the final layout margins change, and assert that no horizontal scrollbar appears.

## 6. Public data model

Keep the existing namespace initially to avoid a risky mechanical rename, but
replace the one-slot/one-display assumption with separate producer and binding
records.

```cpp
// New value members require complete types in llprismlens.h.
#include "llmath.h"
#include "llvector3.h"
// Also include <array>, keep the existing <string>, and forward-declare LLSD.

namespace LLPrismLens
{
constexpr U32 MAX_CAPTURES = 3;
constexpr U32 MAX_DISPLAY_BINDINGS = 16;
constexpr U32 MAX_LENSES = MAX_CAPTURES; // temporary source compatibility

enum class ERegistryResult : U8
{
    OK,
    INVALID_SELECTION,
    DUPLICATE,
    AT_CAPACITY,
    STALE_HANDLE,
    INVALID_CONFIGURATION
};

enum class ECaptureMode : U8
{
    SURFACE_LENS,
    CAMERA_FEED
};

enum class EFitMode : U8
{
    FIT,
    FILL,
    STRETCH
};

enum class EFovMode : U8
{
    FIXED,
    FOLLOW_PROJECTOR
};

enum class EOutputRateMode : U8
{
    AUTOMATIC,
    TARGET_FPS
};

enum class ECaptureHealth : U8
{
    READY,
    UNBOUND_SOURCE,
    SOURCE_OFFLINE,
    INVALID_SOURCE,
    LENS_SURFACE_OFFLINE,
    INVALID_LENS_SURFACE
};

enum class EDisplayHealth : U8
{
    READY,
    OFFLINE,
    INVALID
};

enum class EDisplayVisibility : U8
{
    UNKNOWN,
    OFFSCREEN,
    VISIBLE
};

enum class EOutputState : U8
{
    EMPTY,
    CURRENT,
    HELD,
    SUPPRESSED
};

enum class EActivityState : U8
{
    IDLE,
    WAITING,
    LIVE,
    PAUSED,
    THROTTLED
};

struct CameraSettings
{
    EFovMode mFovMode = EFovMode::FIXED;
    F32 mFixedVerticalFovRad = 60.f * DEG_TO_RAD;
    F32 mNearClip = 0.05f;
    F32 mFarClip = 256.f;
    LLVector3 mLocalEyeOffset;
    F32 mOutputAspect = 16.f / 9.f;
};

struct CaptureRateSettings
{
    EOutputRateMode mMode = EOutputRateMode::AUTOMATIC;
    // Retained while Automatic so switching back restores the last creative
    // choice. Validation requires a finite value in [1, 30].
    F32 mTargetFps = 30.f;
};

struct CaptureRuntimeState
{
    ECaptureHealth mHealth = ECaptureHealth::READY;
    EOutputState mOutput = EOutputState::EMPTY;
    EActivityState mActivity = EActivityState::IDLE;
    std::string mReason;
    F32 mEffectiveVerticalFovRad = 0.f;
    F32 mEffectiveFarClip = 0.f;
    F32 mEffectiveResolutionScale = 0.f;
    F32 mCadenceEntitlementHz = 0.f; // fair per-producer share, not global rate
    F32 mObservedPublicationHz = 0.f;
    F32 mOutputAgeSeconds = 0.f;
};

struct DisplayRuntimeState
{
    EDisplayHealth mHealth = EDisplayHealth::READY;
    EDisplayVisibility mVisibility = EDisplayVisibility::UNKNOWN;
    std::string mHealthReason;
    std::string mVisibilityReason;
};

enum class EPerformanceState : U8
{
    DISABLED,
    LEARNING,
    STEADY,
    PROTECTING,
    SUSPENDED,
    TIMING_UNAVAILABLE
};

struct PerformanceSnapshot
{
    U64 mRevision = 0;
    bool mAdaptiveEnabled = true;
    EPerformanceState mState = EPerformanceState::LEARNING;
    F32 mRequestedProtectedMainFps = 30.f;
    F32 mEffectiveProtectedMainFps = 30.f;
    bool mRequestedTargetAvailable = true;
    F32 mMainRenderP95Milliseconds = 0.f;
    F32 mPresentedFps = 0.f;
    F32 mUserResolutionScaleCeiling = 1.f;
    F32 mRequestedTotalCaptureBudgetHz = 30.f; // 0 means Manual Every Frame
    F32 mEffectiveTotalCaptureBudgetHz = 30.f;
    F32 mMinimumAppliedResolutionScale = 0.f;
    F32 mMaximumAppliedResolutionScale = 0.f;
    F32 mAdmittedGlobalAttemptRateHz = 0.f;
    F32 mObservedGlobalAttemptRateHz = 0.f;
    bool mGpuTimingReliable = false;
    std::string mReason;
};

struct CaptureHandle
{
    LLUUID mId;
    U64 mGeneration = 0;
};

struct DisplayHandle
{
    LLUUID mId;
    U64 mGeneration = 0;
};

struct ActionStatus
{
    ERegistryResult mResult = ERegistryResult::INVALID_SELECTION;
    std::string mReason;
    bool allowed() const { return mResult == ERegistryResult::OK; }
};

struct DisplaySettings
{
    EFitMode mFitMode = EFitMode::FIT;
    F32 mAnchor[2] = { 0.5f, 0.5f };
    F32 mBarColorLinear[3] = { 0.f, 0.f, 0.f };
};

struct CaptureDefinition
{
    U32 mSlot = MAX_CAPTURES; // scheduler/storage detail, not persistent identity
    CaptureHandle mHandle;    // ID persists in scenes; generation never does
    ECaptureMode mMode = ECaptureMode::SURFACE_LENS;
    LLUUID mCameraObjectId; // null while a feed is unbound
    CameraSettings mCamera;
    CaptureRateSettings mRate;
    CaptureRuntimeState mRuntime;
    U32 mDisplayCount = 0;
};

struct DisplayDefinition
{
    DisplayHandle mHandle;
    CaptureHandle mCapture;
    LLUUID mDisplayObjectId;
    S32 mDisplayTextureEntry = -1;
    DisplaySettings mSettings;
    DisplayRuntimeState mRuntime;
};

struct RegistrySnapshot
{
    U64 mConfigurationRevision = 0;
    U64 mRuntimeRevision = 0;
    U32 mCaptureCount = 0;
    U32 mDisplayCount = 0;
    std::array<CaptureDefinition, MAX_CAPTURES> mCaptures;
    std::array<DisplayDefinition, MAX_DISPLAY_BINDINGS> mDisplays;
};

ActionStatus addCameraSelectionStatus();
ActionStatus addLensSelectionStatus();
ActionStatus addDisplaySelectionStatus(const CaptureHandle& capture);
ActionStatus setCameraSelectionStatus(const CaptureHandle& capture);
ERegistryResult addCameraCaptureFromSelectedObject(
    CaptureHandle* capture, std::string* reason = nullptr);
ERegistryResult addSurfaceLensFromSelectedFace(
    CaptureHandle* capture, std::string* reason = nullptr);
ERegistryResult addSelectedDisplay(const CaptureHandle& capture,
    EFitMode fit, DisplayHandle* binding,
    std::string* reason = nullptr);
bool setSelectedCamera(const CaptureHandle& capture,
                       std::string* reason = nullptr);
bool setCameraSettings(const CaptureHandle& capture,
                       const CameraSettings& settings,
                       std::string* reason = nullptr);
bool setCaptureRateSettings(const CaptureHandle& capture,
                            const CaptureRateSettings& settings,
                            std::string* reason = nullptr);
bool setDisplaySettings(const DisplayHandle& binding,
                         const DisplaySettings& settings,
                         std::string* reason = nullptr);
bool removeDisplay(const DisplayHandle& binding,
                   std::string* reason = nullptr);
bool removeCapture(const CaptureHandle& capture);
U64 configurationRevision();
U64 runtimeRevision();
RegistrySnapshot registrySnapshot();
PerformanceSnapshot performanceSnapshot();
LLSD sceneData();
bool applySceneData(const LLSD& data, std::string* reason = nullptr);
}
```

Selection-status APIs are non-mutating and are the single source of truth for
the Add Camera, Add Lens, Add Display, and Set Camera buttons and their tooltips.
Director must not duplicate validation. Creation returns the complete handle;
`registrySnapshot()` provides bounded enumeration and current generations for
manager recovery, while `performanceSnapshot()` exposes global controller state
without overloading a capture row. Every capture and binding receives a nonnull
viewer-local ID plus a new nonzero generation when added or replaced. An editor
retains the returned handle and passes it to mutations; remove/re-add or scene
replacement clears the embedded detail selection and disables mutations instead
of letting a reused array slot mutate another record. The manager stays open and
may safely select a current handle from the fresh snapshot.

Snapshot arrays are densely packed through their corresponding count and are
copied on the viewer thread. Button actions re-run the same validation at commit
time, so a status result is guidance rather than a time-of-check authorization.

Generations come from one process-lifetime monotonic `U64` allocator shared by
captures and displays. Zero is reserved; the counter is never reset by Clear,
scene replacement, logout, or reuse of a persisted UUID. Exhaustion disables
new registry mutations rather than wrapping. Therefore loading the same scene
and persistent IDs cannot accidentally validate an editor from an earlier load.

`configurationRevision()` changes only for user/scene edits. `runtimeRevision()`
changes when health, output, activity, effective projector FOV/far clip, or
effective adaptive scale/entitlement or reason changes. Age and live p95 are
quantized/polled UI values rather than per-frame revisions; the open manager
refreshes at most four times/second and age text at most once/second. Display
runtime changes use the same runtime revision. `PerformanceSnapshot::mRevision`
changes for quantized global controller state, requested/effective target and
total budget, applied-scale range, admitted/observed attempt rate, timing
reliability, or reason; it does not pretend a global rate is any one producer's
refresh rate.

The internal registry has two fixed arrays. Only captures own render targets:

```cpp
struct CameraPose
{
    LLVector3 mEye;
    LLVector3 mAt;
    LLVector3 mLeft;
    LLVector3 mUp;
    F32 mVerticalFovRad = 60.f * DEG_TO_RAD;
    F32 mNearClip = 0.05f;
    F32 mFarClip = 256.f;
};

struct AdaptiveCaptureState
{
    F32 mEffectiveScale = 1.f;
    F32 mCadenceEntitlementHz = 0.f;
    F32 mObservedPublicationHz = 0.f;
    F64 mNextSteadyDueTime = 0.0;
    F64 mLastAttemptTime = 0.0;
    F64 mLastProducedTime = 0.0;
    F64 mRetryAfterTime = 0.0;
    F64 mCostModelValidUntil = 0.0;
    F64 mProbeDueTime = 0.0;
};

struct GlobalCadenceState
{
    F32 mEffectiveAttemptRateHz = 0.f;
    F64 mLastGlobalAttemptTime = 0.0;
    F64 mNextGlobalSteadyDueTime = 0.0;
    bool mManualEveryFrame = false;
};

struct CaptureRecord
{
    bool mOccupied = false;
    U64 mGeneration = 0;
    LLUUID mCaptureId;
    LLPrismLens::ECaptureMode mMode = LLPrismLens::ECaptureMode::SURFACE_LENS;
    LLUUID mCameraObjectId;
    LLPrismLens::CameraSettings mCameraSettings;
    LLPrismLens::CaptureRateSettings mRateSettings;
    LLPrismLens::CaptureRuntimeState mRuntime;
    LLUUID mPrimaryLensBindingId; // required/exclusive for SURFACE_LENS

    CameraPose mResolvedCamera;
    bool mCapturePrepared = false;
    bool mAnyDisplayVisible = false;
    U32 mDesiredOutputWidth = 0;
    U32 mDesiredOutputHeight = 0;

    bool mHasOutput = false;
    U32 mOutputWidth = 0;
    U32 mOutputHeight = 0;
    U32 mOutputRevision = 0;
    F32 mTextureRegionScale[2] = { 1.f, 1.f };
    F32 mTextureRegionOffset[2] = { 0.f, 0.f };
    F32 mRetainedOrientationScale[2] = { 1.f, 1.f };
    F32 mRetainedOrientationOffset[2] = { 0.f, 0.f };
    CameraPose mLastResolvedPose;
    bool mHaveLastResolvedPose = false;
    AdaptiveCaptureState mAdaptive;
};

struct DisplayRecord
{
    bool mOccupied = false;
    U64 mGeneration = 0;
    LLUUID mBindingId;
    LLUUID mCaptureId;
    LLUUID mDisplayObjectId;
    S32 mDisplayTE = -1;
    LLPrismLens::DisplaySettings mSettings;
    LLPrismLens::DisplayRuntimeState mRuntime;
    PrismFrame mFrame; // current surface basis and screen footprint
    F32 mDisplayToCaptureScale[2] = { 1.f, 1.f };
    F32 mDisplayToCaptureOffset[2] = { 0.f, 0.f };
    F32 mContentUvMin[2] = { 0.f, 0.f }; // FIT bar/discard rectangle
    F32 mContentUvMax[2] = { 1.f, 1.f };
    U32 mDemandWidth = 0;
    U32 mDemandHeight = 0;
};

std::array<CaptureRecord, MAX_CAPTURES> mCaptures;
std::array<DisplayRecord, MAX_DISPLAY_BINDINGS> mDisplays;
```

The separation is normative:

- resolving sixteen faces never creates sixteen scene renders;
- a capture is eligible only if its source/aperture is ready and at least one
  binding is visible;
- each healthy visible binding independently composites its producer's current
  or held output;
- one offline/invalid binding cannot suppress siblings or erase its capture;
- adding a face may show the producer's current image immediately, then request
  a higher-resolution refresh if that face raises aggregate pixel demand;
- removing the last binding idles the producer. After a short idle retention
  period (suggested five seconds), release its output and report `EMPTY/IDLE`;
  the configured capture remains and still consumes one of three slots.

Rate ownership follows the same boundary. `setCaptureRateSettings()` updates one
producer and therefore all of its displays. It must validate the complete
settings value before mutation, preserve the retained output and valid cost
history, recompute scheduler entitlement, increment configuration/runtime
revisions, and create no accumulated deadline credit. `setDisplaySettings()` has
no rate parameter. Adding or removing a display creates no extra rate request or
scheduler vote; only its ordinary composite cost can indirectly reduce admitted
global headroom.

The rate type is shared by `SURFACE_LENS` and `CAMERA_FEED`. A Lens simply has one
required display; a Camera Feed may fan the same publication cadence to many.
`removeDisplay()` rejects a Surface Lens's primary binding with a stable reason;
the confirmed `removeCapture()` cascade removes that capture and its required
binding atomically. Camera Feed bindings retain ordinary independent removal.

The output-state lifecycle is normative:

- transient source loss with a prior output -> `HELD`, composite the frozen
  output on every healthy binding;
- transient source loss with no output -> `EMPTY/WAITING`, show base material;
- adaptive downshift while still updating -> `CURRENT/THROTTLED`; adaptive
  suspension with a valid output -> `HELD/PAUSED`, keep the same
  synchronized frame on every healthy binding;
- invalid source, rebind, optics/aspect edit, or detected camera cut ->
  `SUPPRESSED`, immediately show base material on every binding even if the
  texture allocation remains;
- only a successful capture published after full state restoration ->
  `CURRENT/LIVE` and clears suppression;
- `getCompositeStates()` may emit up to sixteen bindings, but only when their
  producer is `CURRENT` or `HELD` and their own geometry is healthy/visible;
- all bindings sample the same retained texture generation, so the screens are
  frame-synchronized.

## 7. Source-selection validation

Source selection must iterate the entire selection, as the current
`selectedFaceIdentity()` does. Do not use only `getPrimaryObject()`: that could
turn an ambiguous multi-selection into an apparently valid source.

Require:

- exactly one selected object;
- a non-dead `LLVOVolume`;
- not a HUD attachment;
- not rigged or animesh-driven;
- a finite render position and rotation;
- `isLightSpotlight()` only when `FOLLOW_PROJECTOR` is requested.

User edits and scene input are validated, not silently repaired: fixed FOV must
be finite and within 5–175 degrees; near clip within 0.01–10 m; configured far
clip within 0.2–512 m and at least `near + 0.1`; every eye-offset component must
be finite; canonical output aspect must be finite within 0.25–4.0. A live
followed-projector FOV outside the same supported range makes the source invalid
instead of quietly changing the projector's optics. The effective far plane is
additionally capped at render time by the copied main camera's usable far plane
and is exposed in runtime state.

Allow non-HUD attachments. This enables a wearable or vehicle-mounted camera
and `getRenderPosition()` / `getRenderRotation()` already provide the relevant
live rendered pose.

One camera capture drives all of its display bindings. When adding a binding,
reject a display-object UUID equal to that capture's source UUID and reject any
`(UUID, TE)` already owned by another producer. The same source object may back
two distinct capture records only when the creator intentionally needs different
optics/aspects; warn that this performs two expensive scene renders.

Rebinding a capture performs the inverse whole-set check: reject a selected
source whose UUID equals any existing display object under that capture.

Binding or rebinding a source, changing FOV mode, or editing fixed lens values
or output aspect must invalidate that capture's retained output immediately.
Every bound face shows base material until the new shared capture publishes.

## 8. Resolving the Blender/projector-compatible camera pose

The source object's pivot is the optical center, plus an explicit local offset.
Do not silently copy the projector's scale-derived cone apex: resizing a marker
would then move the camera. Marker visibility uses the explicit transparent-
marker/forward-eye contract described later; no incomplete batch-hiding hook is
assumed.

```cpp
enum class ECameraResolveResult : U8
{
    READY,
    TRANSIENT,
    INVALID
};

ECameraResolveResult resolveCameraPose(const CaptureRecord& capture,
                                       F32 usable_far,
                                       CameraPose& pose,
                                       std::string& reason)
{
    LLViewerObject* object = gObjectList.findObject(capture.mCameraObjectId);
    if (!object || object->isDead())
    {
        reason = "camera source is temporarily unavailable";
        return ECameraResolveResult::TRANSIENT;
    }

    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume || object->isHUDAttachment() || object->isRiggedMesh() ||
        volume->isAnimatedObject())
    {
        reason = "camera source must be a non-HUD, static, non-rigged volume";
        return ECameraResolveResult::INVALID;
    }

    LLQuaternion rotation = object->getRenderRotation();
    if (!rotation.isFinite() || rotation.normalize() <= F_ALMOST_ZERO)
    {
        reason = "camera source has an invalid render rotation";
        return ECameraResolveResult::TRANSIENT;
    }

    LLVector3 local_offset = capture.mCameraSettings.mLocalEyeOffset;
    local_offset *= rotation;
    pose.mEye = object->getRenderPosition() + local_offset;

    pose.mAt.setVec(0.f, 0.f, -1.f);
    pose.mUp.setVec(0.f, 1.f, 0.f);
    pose.mAt *= rotation;
    pose.mUp *= rotation;
    if (pose.mAt.normVec() <= F_ALMOST_ZERO)
    {
        reason = "camera source has a degenerate forward axis";
        return ECameraResolveResult::TRANSIENT;
    }

    // LLCamera stores at/left/up. For local -Z forward and +Y up,
    // camera-left is local -X.
    pose.mLeft = pose.mUp % pose.mAt;
    if (pose.mLeft.normVec() <= F_ALMOST_ZERO)
    {
        reason = "camera source has a degenerate up axis";
        return ECameraResolveResult::TRANSIENT;
    }
    pose.mUp = pose.mAt % pose.mLeft;
    pose.mUp.normVec();

    F32 fov = capture.mCameraSettings.mFixedVerticalFovRad;
    if (capture.mCameraSettings.mFovMode ==
        LLPrismLens::EFovMode::FOLLOW_PROJECTOR)
    {
        if (!volume->isLightSpotlight())
        {
            reason = "Follow projector FOV requires a projector source";
            return ECameraResolveResult::INVALID;
        }
        fov = volume->getSpotLightParams().mV[VX];
    }

    if (!std::isfinite(fov) || fov < 5.f * DEG_TO_RAD ||
        fov > 175.f * DEG_TO_RAD)
    {
        reason = "camera field of view is outside the supported range";
        return ECameraResolveResult::INVALID;
    }

    pose.mVerticalFovRad = fov;
    pose.mNearClip = capture.mCameraSettings.mNearClip; // validated on mutation
    const F32 minimum_far = llmax(MIN_FAR_PLANE, pose.mNearClip + 0.1f);
    const F32 renderer_far = llmin(usable_far, 512.f);
    if (!std::isfinite(renderer_far) || renderer_far < minimum_far)
    {
        reason = "current viewer far range does not reach beyond the near clip";
        return ECameraResolveResult::TRANSIENT;
    }
    pose.mFarClip = llmin(capture.mCameraSettings.mFarClip, renderer_far);

    if (!pose.mEye.isFinite() || !std::isfinite(pose.mVerticalFovRad) ||
        !std::isfinite(pose.mNearClip) || !std::isfinite(pose.mFarClip))
    {
        reason = "camera source produced non-finite optics";
        return ECameraResolveResult::TRANSIENT;
    }
    return ECameraResolveResult::READY;
}
```

In this checkout `LLQuaternion::normalize()` returns the pre-normalization
magnitude, so the finite/degenerate test above matches the local API contract.

## 9. Projection: ordinary symmetric perspective

Camera-feed mode does not use the current Kooima-style surface aperture. The
capture has one canonical aspect independent of every destination face:

```cpp
const F32 capture_aspect = capture.mCameraSettings.mOutputAspect; // validated

const F32 half_height = pose.mNearClip *
    tanf(0.5f * pose.mVerticalFovRad);
const F32 half_width = half_height * capture_aspect;

const glm::mat4 projection = glm::frustum(
    -half_width, half_width,
    -half_height, half_height,
    pose.mNearClip, pose.mFarClip);

const glm::vec3 eye(pose.mEye.mV[VX],
                    pose.mEye.mV[VY],
                    pose.mEye.mV[VZ]);
const glm::vec3 at(pose.mAt.mV[VX],
                   pose.mAt.mV[VY],
                   pose.mAt.mV[VZ]);
const glm::vec3 up(pose.mUp.mV[VX],
                   pose.mUp.mV[VY],
                   pose.mUp.mV[VZ]);
const glm::mat4 modelview = glm::lookAt(eye, eye + at, up);
```

The producer aggregates the texel demand of all healthy visible bindings, then
applies the effective adaptive scale. Target allocation preserves the canonical
aspect exactly; it never uses one binding's projected AABB ratio as camera
optics. Use a 64–1024 logical-extent range per axis and stable demand buckets.
The largest visible screen can raise shared capture resolution, but it still
cannot create another scene render.

Each capture owns a lazily allocated scratch pack whose physical dimensions must
equal that capture's scheduled render dimensions. Do not render a logical
subrect of a larger sampler2D allocation: fullscreen deferred shaders generate
UV 0-1, so viewport/scissor and `screen_res` alone cannot remap normalized
sampling and would read stale pixels. Exact-size per-capture packs avoid both
that correctness fault and per-frame allocation churn when differently sized
captures alternate. Matching exact-size Prism water-depth and water-exclusion
targets are part of the same contract.

Install the temporary camera completely:

```cpp
LLViewerCamera feed_camera = main_camera;
feed_camera.setOrigin(pose.mEye);
feed_camera.setAxes(pose.mAt, pose.mLeft, pose.mUp);
feed_camera.setAspect(capture_aspect);
feed_camera.setViewHeightInPixels(static_cast<S32>(render_height));
feed_camera.setViewNoBroadcast(pose.mVerticalFovRad);
feed_camera.setNear(pose.mNearClip);
feed_camera.setFar(pose.mFarClip);
feed_camera.disableUserClipPlane();
LLViewerCamera::instance() = feed_camera;

set_current_projection(projection);
set_current_modelview(modelview);
gGL.matrixMode(LLRender::MM_PROJECTION);
gGL.loadMatrix(glm::value_ptr(projection));
gGL.matrixMode(LLRender::MM_MODELVIEW);
gGL.loadMatrix(glm::value_ptr(modelview));
```

`setViewNoBroadcast()` is mandatory. `LLViewerCamera::setView()` sends an
`AgentFOV` message and an auxiliary local camera must never produce simulator
camera traffic.

Because the projection is symmetric, the `LLViewerCamera` FOV/aspect/near/far
frustum matches the GL matrix directly. Do not update it at this point, however:
`updateFrustumPlanes()` also reads `gGLViewport`. Invoke it only after the scoped
logical viewport is installed, in the order shown in Section 11:

```cpp
LLViewerCamera::updateFrustumPlanes(
    LLViewerCamera::instance(), false, false, true);
```

No conservative off-axis cull envelope is needed for feed mode.

## 10. Binding-first preparation and capture aggregation

Retain current face validation and projected-footprint calculation, but do not
encode visibility as health and do not resolve one camera once per face.
Preparation has three phases: clear capture aggregates, resolve every display,
then resolve each producer exactly once.

```cpp
enum class EDisplayResolveResult : U8
{
    READY,
    TRANSIENT,
    INVALID
};

struct DisplayPreparation
{
    EDisplayResolveResult mHealth = EDisplayResolveResult::TRANSIENT;
    bool mVisible = false;
    std::string mHealthReason;
    std::string mVisibilityReason;
};

for (CaptureRecord& capture : mCaptures)
{
    capture.mAnyDisplayVisible = false;
    capture.mDesiredOutputWidth = 0;
    capture.mDesiredOutputHeight = 0;
}

for (DisplayRecord& binding : mDisplays)
{
    if (!binding.mOccupied)
    {
        continue;
    }

    CaptureRecord* capture = findCapture(binding.mCaptureId);
    const DisplayPreparation display = prepareDisplaySurface(
        binding, viewport, main_projection, main_modelview, main_camera);

    binding.mRuntime.mHealth = display.mHealth == EDisplayResolveResult::READY
        ? EDisplayHealth::READY
        : (display.mHealth == EDisplayResolveResult::TRANSIENT
               ? EDisplayHealth::OFFLINE : EDisplayHealth::INVALID);
    binding.mRuntime.mVisibility = binding.mRuntime.mHealth != EDisplayHealth::READY
        ? EDisplayVisibility::UNKNOWN
        : (display.mVisible ? EDisplayVisibility::VISIBLE
                            : EDisplayVisibility::OFFSCREEN);
    binding.mRuntime.mHealthReason = display.mHealthReason;
    binding.mRuntime.mVisibilityReason = display.mVisibilityReason;

    if (!capture ||
        binding.mRuntime.mVisibility != EDisplayVisibility::VISIBLE)
    {
        continue;
    }

    computeBindingFitAndDemand(binding, capture->mMode,
                               capture->mCameraSettings.mOutputAspect);
    capture->mAnyDisplayVisible = true;
    accumulateDesiredCaptureExtent(*capture, binding); // maximum, never sum
}

for (CaptureRecord& capture : mCaptures)
{
    if (!capture.mOccupied)
    {
        continue;
    }

    if (capture.mMode == ECaptureMode::SURFACE_LENS)
    {
        prepareLensProducerFromPrimaryBinding(capture, main_camera);
    }
    else
    {
        resolveCameraProducer(capture, main_camera.getFar()); // exactly once
    }

    if (!capture.mAnyDisplayVisible)
    {
        capture.mRuntime.mActivity = EActivityState::IDLE;
    }
}
```

`prepareDisplaySurface()` first resolves object, face, topology, and basis. Only
a healthy display proceeds to its independent visibility/minimum-area test. A
valid offscreen/tiny display is `READY + OFFSCREEN`, not `OFFLINE`; an unresolved
or invalid display reports `UNKNOWN` visibility because no footprint was tested.

For Camera Feed, a bad binding changes only its `DisplayRuntimeState`. For Lens,
the sole binding is also its optical aperture, so failure maps to
`LENS_SURFACE_OFFLINE`/`INVALID_LENS_SURFACE` and may suppress that producer.
Resolve camera sources even when every screen is offscreen (while Prism is
enabled) so status and cut detection remain truthful.

Do not remove a capture because its source temporarily unloads, and do not
remove a binding because its face disappears. Invalid source/optics suppress
the shared capture; a bad display shows base material without affecting sibling
screens. Replace current `rejectSlot()` auto-deletion with typed runtime failure.
Only explicit Remove/Clear or an atomically validated scene replacement erases
configuration.

## 11. Mode-specific auxiliary render branch

The existing scheduled render remains one function with a producer-mode branch.
It receives a capture slot; display bindings never call it:

```cpp
CaptureRecord& capture = mCaptures[capture_slot];
if (capture.mMode == ECaptureMode::SURFACE_LENS)
{
    DisplayRecord& aperture = requireDisplay(capture.mPrimaryLensBindingId);
    installLensOffAxisCamera(capture, aperture, main_camera,
                             projection, modelview, aux_camera);
}
else
{
    installCameraFeed(capture, main_camera,
                      projection, modelview, aux_camera);
}

LLViewerCamera::instance() = aux_camera;
set_current_projection(projection);
set_current_modelview(modelview);

// Registers a scoped active logical extent, installs matrices plus the actual
// viewport/scissor/gGLViewport, and overrides logical-resolution queries.
PrismLogicalExtentScope logical_extent(gPipeline,
                                       render_width, render_height);
logical_extent.installForCurrentTarget(projection, modelview);

// updateFrustumPlanes() reads gGLViewport: do this only after the logical
// viewport above is installed, and before auxiliary cull.
LLViewerCamera::updateFrustumPlanes(
    LLViewerCamera::instance(), false, false, true);

refreshAuxEnvironmentUniforms();

static LLCullResult prism_cull;
prism_cull.clear();
gPipeline.updateCull(LLViewerCamera::instance(), prism_cull);
gPipeline.stateSort(LLViewerCamera::instance(), prism_cull);

LLPipeline::RenderTargetPack& rt = gPipeline.mPrismLensRT[capture_slot];
// bindTarget() resets viewport state. This helper calls bindTarget() and then
// immediately reapplies viewport/scissor/gGLViewport for the exact-size pack.
gPipeline.bindPrismLensTarget(rt.deferredScreen);
glClearColor(0.f, 0.f, 0.f, 0.f);
rt.deferredScreen.clear();
gPipeline.renderGeomDeferred(LLViewerCamera::instance(), false);
rt.deferredScreen.flush();
gPipeline.renderDeferredLighting(); // every nested FBO bind restores exact extent

gPipeline.mPrismLensOutput[capture_slot].copyContents(
    rt.screen,
    0, 0, render_width, render_height,
    0, 0, output_width, output_height,
    GL_COLOR_BUFFER_BIT,
    render_width == output_width && render_height == output_height
        ? GL_NEAREST : GL_LINEAR);
```

The extent scope is not a one-time pre-bind viewport call. Every direct and
nested FBO bind in geometry, deferred lighting, post, and water must route
through the Prism bind/reapply helpers. While the scope is active, every relevant
screen-size/inverse-size/noise-step uniform reads `render_width/render_height`.
The pack and its water targets have exactly those dimensions, so normalized UVs
and texel-size uniforms agree. The scope restores the original GL viewport,
scissor, `gGLViewport`, resolution source, matrices, and FBO on exit.

Keep every return after state mutation inside the existing complete RAII scope.
Publish `mHasOutput`, output dimensions, logical-region transform, and retained
capture-time orientation metadata only after the
scope has restored the main camera, matrices, viewport, FBO, shader, masks, and
environment uniforms. Increment one capture-output revision; every display
binding observes that same publication.

## 12. Camera-feed clipping policy

The destination face is not an optical aperture for a remote camera. Its plane
is unrelated to the source scene and would cut arbitrary content.

For `CAMERA_FEED`:

- call `disableUserClipPlane()` on the copied camera;
- do not build or install `mFragmentClipPlane`;
- make `getActiveClipPlane()` return false;
- keep `mirror_flag`/`CLIP_PLANE` disabled for the auxiliary feed;
- do not apply the destination-face plane, but still run the renderer's normal
  source-camera water-plane clip when `isWaterClip()` requires it.

```cpp
bool getActiveClipPlane(LLPlane& plane)
{
    const CaptureRecord* active = registry.activeCapture();
    if (!LLPipeline::sPrismLensRender || !active ||
        active->mMode != ECaptureMode::SURFACE_LENS)
    {
        return false;
    }
    const DisplayRecord* aperture =
        registry.findDisplay(active->mPrimaryLensBindingId);
    if (!aperture)
    {
        return false;
    }
    plane = aperture->mFrame.mFragmentClipPlane;
    return true;
}
```

`LLPipeline::updateCull()` must be mode-aware: install the custom Prism plane
only when `getActiveClipPlane()` succeeds; otherwise fall through to the
ordinary water-clip branch. The current blanket `sPrismLensRender` branch skips
that ordinary branch and is not correct for a remote feed.

Rename `activateClipUniforms()` to `refreshAuxEnvironmentUniforms()`. Camera
feeds still need an environment-uniform rebuild after installing the source
camera, even though they have no custom clip plane.

## 13. Optical medium follows the source camera

This differs from the rejected mirror design. A camera feed is a real virtual
viewpoint, so fog, water-plane height, and underwater pool ordering must follow
the **source eye**, not the user's main eye.

Pass `pose.mEye` into the active-capture scope for feed mode. For Lens mode retain
the main eye. `LLSettingsVOWater::applySpecial()` must use that optical eye for:

- region lookup;
- local water height;
- underwater decision;
- eye-space water plane;
- water fog density.

Also set `LLPipeline::sUnderWaterRender` for the auxiliary pass before draw-pool
ordering is chosen. The current render-state scope already saves/restores it.

```cpp
bool cameraUnderLocalWater(const LLVector3& eye)
{
    LLViewerRegion* region = LLWorld::instance().getRegionFromPosAgent(eye);
    const F32 water_height = region
        ? region->getWaterHeight()
        : LLEnvironment::instance().getWaterHeight();
    return eye.mV[VZ] <= water_height;
}
```

A camera in another region still uses the viewer's currently loaded
environment state; fully independent per-region EEP settings would be a larger
environment-system change.

## 14. Camera-marker visibility contract

Unlike a Blender camera object, an SL source prim is ordinary renderable
geometry. Do **not** claim that an early return from
`stateSort(LLDrawable*, LLCamera&)` hides it: static spatial groups can submit
already-built `LLDrawInfo` later, and opaque/alpha/rigged paths are not all
covered by that hook.

The safe first-version contract is therefore explicit:

- the source prim is not automatically removed from its own feed;
- creators use a dedicated fully transparent marker prim, or place the local
  eye offset in front of a visible camera housing;
- the selected-capture editor provides `Place eye in front`, setting local Z to
  `-(0.5 * source_scale_z + near_clip + 0.01)` metres, and warns when the
  resolved eye lies inside the source's render bounds;
- a future automatic-hide option requires submission-level exclusion that
  covers cached static groups, opaque, alpha, and rigged draw paths; it must not
  ship as a partial `stateSort()` filter.

Reject any binding whose display-object UUID equals its parent capture's source
UUID. Screen/source feedback semantics are confusing even when recursive Prism
composites are suppressed, and a forward eye offset cannot make that binding
unambiguous.

Suppress every Prism composite while an auxiliary Prism render is active. A
feed pointed at another Prism screen therefore sees that screen's base material,
not an unbounded camera-to-camera recursion. A deliberate one-frame-delayed
recursion mode can be designed later, but it must not be accidental.

## 15. Fan-out compositor, orientation, and Fit/Fill

Camera Feed output orientation is canonical and capture-owned. Lens remains
side-dependent: at successful publication, copy the capture-time U
scale/offset into the capture's retained `mRetainedOrientationScale/Offset`.
Never recompute it from the aperture's current side while sampling an older
texture, because crossing the surface before the next capture would mirror the
held image. Camera Feed publishes identity orientation. Each Camera Feed display
owns only its surface basis and mapping from display UV to the same canonical
capture UV.

Let `Ac` be capture aspect, `Ad` the binding's physical
`|world U edge| / |world V edge|`, `d` the display UV, and `a` its anchor
(default center `{0.5,0.5}`). Precompute:

```cpp
// FIT: entire source visible; unused destination area becomes bars.
const LLVector2 content(llmin(1.f, Ac / Ad),
                        llmin(1.f, Ad / Ac));
LLVector2 fit_uv;
fit_uv.mV[VX] = (d.mV[VX] - (1.f - content.mV[VX]) * anchor.mV[VX]) /
                 content.mV[VX];
fit_uv.mV[VY] = (d.mV[VY] - (1.f - content.mV[VY]) * anchor.mV[VY]) /
                 content.mV[VY];
// fit_uv outside [0,1] is bar color, never clamped edge smear.

// FILL: destination filled; q is the retained source crop.
const LLVector2 crop(llmin(1.f, Ad / Ac),
                     llmin(1.f, Ac / Ad));
LLVector2 fill_uv;
fill_uv.mV[VX] = (1.f - crop.mV[VX]) * anchor.mV[VX] +
                  d.mV[VX] * crop.mV[VX];
fill_uv.mV[VY] = (1.f - crop.mV[VY]) * anchor.mV[VY] +
                  d.mV[VY] * crop.mV[VY];

// STRETCH
const LLVector2 stretch_uv = d;
```

A new fragment path is required for real Fit. The current shader clamps UV and
would smear an edge texel through letterbox/pillarbox regions. Shader order is:

```glsl
vec2 logical_uv = prism_uv * displayToCaptureScale +
                   displayToCaptureOffset;
if (letterbox != 0 &&
    (any(lessThan(logical_uv, vec2(0.0))) ||
     any(greaterThan(logical_uv, vec2(1.0)))))
{
    frag_color = vec4(barColorLinear, 0.0); // alpha remains Prism glow channel
}
else
{
    vec2 oriented_uv = logical_uv * retainedOrientationScale +
                       retainedOrientationOffset;
    vec2 texture_uv = oriented_uv * textureRegionScale + textureRegionOffset;
    frag_color = texture(prismLensMap, texture_uv);
}
```

`prism_uv` is the existing vertex/fragment interface. The two retained
orientation uniforms are capture-owned publication metadata; the logical-region
transform then maps that oriented image into the valid subrectangle of the
physical retained texture.

Use projected raw U/V edge lengths, not only the screen AABB, to estimate texel
demand. If `s` is the absolute display-to-capture scale:

```cpp
binding.mDemandWidth = llceil(projected_u_edge_pixels /
                              llmax(s.mV[VX], F_ALMOST_ZERO));
binding.mDemandHeight = llceil(projected_v_edge_pixels /
                               llmax(s.mV[VY], F_ALMOST_ZERO));

capture_need_w = max(binding demand widths);  // never sum
capture_need_h = max(binding demand heights);
ideal_h = max(capture_need_h, capture_need_w / Ac);
ideal_w = Ac * ideal_h;
// Apply effective quality uniformly, preserve aspect, bucket, cap to 1024/axis.
```

Conservatively cap partially clipped edge estimates at 1024. Enforce aspect and
the 64-pixel minimum together so neither axis clamp silently distorts the camera
image.

Use bucketed capture demand so exact-size scratch reallocations occur only at
meaningful resolution transitions. Each capture caches its own pack, so mixed
sizes do not reallocate when the scheduler alternates producers. Retained output
textures are preallocated to fixed 1024-square capacity and publish a logical
region; a scratch allocation failure therefore leaves the old retained frame
and all sibling displays intact, then backs off only that producer.

`CompositeState` contains `mCaptureSlot` (texture owner), binding basis,
display-to-capture transform, capture-owned retained orientation, retained
logical-region transform, letterbox flag, and bar color. `getCompositeStates()` scans at most sixteen bindings but indexes
only the three capture outputs. Sort/group visible states by capture slot so one
texture bind can serve sibling faces; basis/mapping uniforms still change per
face. Never index a three-element output array with a display index. Preserve
the current-call-only `LLFace*` contract and hold resolved face references valid
until the complete composite loop finishes.

Because the compositor remains two-sided, viewing the physical back naturally
resembles the back of a textured card. A front-only option should suppress the
back composite, not introduce another texture flip.

## 16. Fair scheduling and stale-image policy

Preserve the hard rule: no more than one auxiliary scene render in a main
frame. Prepare up to sixteen bindings, aggregate them into at most three
producers, then choose one producer.

Each producer owns one monotonic steady deadline derived from its admitted
entitlement. In Adaptive and numeric-Manual modes, a steady attempt is due only
when **both** the global auxiliary deadline and that producer's
`mNextSteadyDueTime` are due. Manual Every Frame substitutes the current main
frame's one token for the global deadline; the producer deadline still applies.
Among due steady captures choose the most-overdue producer deadline, then oldest
attempt, then aggregated requested target area, with a rotating slot as the final
tie-breaker.
Binding count and summed screen area are never priority—ten screens do not give
one camera ten votes.

Do not create an unbounded urgent class. A new EMPTY producer or one becoming
visible may initialize its deadline to `now` for one prompt first picture. Rebind,
optics edit, and camera cut suppress the displayed texture immediately but do not
accumulate extra turns; a source that cuts every frame cannot starve two steady
cameras. Rate edits likewise change the next deadline without blanking the
retained output, reallocating a target, resetting valid cost history, or granting
backlog credit.

Unbound, invalid, retry-delayed, source-offline, or offscreen entries do not
consume the render budget.

```cpp
bool captureEligible(const CaptureRecord& capture, F64 now)
{
    return capture.mOccupied &&
           capture.mAnyDisplayVisible &&
           capture.mCapturePrepared &&
           now >= capture.mAdaptive.mRetryAfterTime;
}

enum class EAttemptKind : U8 { NONE, STEADY, DISCOVERY_PROBE };

struct AttemptChoice
{
    S32 mSlot = -1;
    EAttemptKind mKind = EAttemptKind::NONE;
    explicit operator bool() const { return mSlot >= 0; }
};

AttemptChoice chooseRenderSlot(F64 now)
{
    S32 best = -1;
    EAttemptKind best_kind = EAttemptKind::NONE;
    F64 best_overdue = -1.0;
    F64 best_attempt_age = -1.0;
    U64 best_area = 0;
    for (U32 offset = 0; offset < MAX_CAPTURES; ++offset)
    {
        const U32 slot = (mNextRenderSlot + offset) % MAX_CAPTURES;
        const CaptureRecord& capture = mCaptures[slot];
        const EAttemptKind kind = !captureEligible(capture, now)
            ? EAttemptKind::NONE
            : gPrismPerformance.attemptKind(capture, now);
        // STEADY requires the producer deadline plus either the numeric global
        // deadline or Manual Every Frame's one token for this main frame.
        // DISCOVERY_PROBE uses its isolated-learning deadline and may run when
        // the steady global budget is zero.
        if (kind == EAttemptKind::NONE)
        {
            continue;
        }
        const F64 due = kind == EAttemptKind::STEADY
            ? capture.mAdaptive.mNextSteadyDueTime
            : capture.mAdaptive.mProbeDueTime;
        const F64 overdue = now - due;
        const F64 attempt_age = now - capture.mAdaptive.mLastAttemptTime;
        const U64 area = U64(capture.mDesiredOutputWidth) *
                         U64(capture.mDesiredOutputHeight);
        if (best < 0 || overdue > best_overdue ||
            (overdue == best_overdue && attempt_age > best_attempt_age) ||
            (overdue == best_overdue && attempt_age == best_attempt_age &&
             area > best_area))
        {
            best = static_cast<S32>(slot);
            best_kind = kind;
            best_overdue = overdue;
            best_attempt_age = attempt_age;
            best_area = area;
        }
    }
    if (best >= 0)
    {
        // Set before allocation/cull/render. A failing producer receives backoff and
        // cannot monopolize the scheduler.
        mCaptures[best].mAdaptive.mLastAttemptTime = now;
        gPrismPerformance.consumeAttemptDeadline(mCaptures[best], best_kind, now);
        mNextRenderSlot = (static_cast<U32>(best) + 1) % MAX_CAPTURES;
    }
    return { best, best_kind };
}
```

The caller must carry `AttemptChoice::mKind` through target selection, render,
timing attribution, and result accounting. A discovery probe always uses the
minimum learning tier and its special classification path; returning only the
slot would silently turn it into an ordinary steady attempt.

`attemptKind()` returns `STEADY` only for a positive entitlement and a due
producer deadline, with the global opportunity evaluated as:

```text
global_due = manual_every_frame
    ? token_available_this_main_frame
    : now >= next_global_steady_due
```

On **every** ordinary attempt, including allocation/cull/render failure, advance
the selected producer deadline. Numeric modes also advance the global deadline;
Manual Every Frame consumes the current frame token and has no global period. If
exactly one deadline period remains current, preserve phase; if multiple periods
were missed, set the next deadline from `now` and discard every skipped period.
Never perform a catch-up attempt.

When requested rate, entitlement, mode, visibility, or Adaptive/Manual state
changes, recompute the entitlement and initialize:

```text
next_steady_due = max(now, last_attempt + 1 / new_entitlement)
```

A zero entitlement has no ordinary deadline. A successful discovery probe sets
the ordinary next deadline from the probe publication time, preventing a probe
from being followed immediately by a steady attempt. Failures consume their
cadence opportunity and also receive the existing bounded retry backoff.

Detect teleport-like source cuts from the change between consecutive resolved
poses, not the difference from the last captured pose. That prevents ordinary
continuous motion plus a 2–3 frame refresh delay from being mistaken for a cut.
Suggested starting thresholds are 2 m translation or 30 degrees rotation in a
single main frame. Set output state to `SUPPRESSED` immediately and keep it
suppressed until its next producer deadline. Once
that deadline and the global opportunity are due, most-overdue selection bounds
only the additional delay from other due captures; the configured picture period,
admission, and retry backoff remain part of cut latency.

Cadence is producer-based. With one visible producer requesting 24 FPS and
sufficient total budget, all of its screens receive the same retained texture
revision at up to 24 picture updates/second. With three 30-FPS requests under a
30-attempt/s budget, max-min allocation admits 10 FPS each. A 5-FPS camera next
to a 30-FPS camera receives 5 and leaves up to 25 for the latter; unused demand
is never turned into needless work. Adaptive admission may reduce any
entitlement further to protect the main-view target.

After the full auxiliary RAII scope restores main state, successful publication
sets `mLastProducedTime`, `CURRENT`, effective optics, and clears the reason.
Activity becomes `LIVE` at requested scale/cadence or `THROTTLED` whenever
effective scale or entitlement is below the current request, whether the limit
came from Adaptive protection or a Manual total budget. Allocation/cull/render
failure does not publish; it retains an allowed
old output as `HELD` (or remains `EMPTY/SUPPRESSED`), sets retry backoff, and
bumps the runtime revision.

## 17. Render-state containment required for a remote view

The current state scope is the correct foundation, but remote camera feeds make
several inherited weaknesses more visible.

### 17.1 Nearby lights

`LLPipeline::calcNearbyLights()` mutates the global `mNearbyLights`, drawable
`NEARBY_LIGHT` flags, visibility marks, and fade history. Running it with the
source camera can contaminate the following main view. Simply copying the set
back is not enough if drawable flags or fades were touched.

For quality, construct an auxiliary light set without touching flags or main
fade history, swap it into `mNearbyLights` under RAII, and restore the main set.
The helper must duplicate the eligibility filters from `calcNearbyLights()` but
must not call `setState`, `clearState`, or `setVisible`. It must also ignore the
drawable's global `NEARBY_LIGHT` membership bit: those bits still describe the
saved main list after the swap. Deduplicate only against the temporary set, and
initialize each temporary `Light` with `fade >= LIGHT_FADE_TIME`; the default
zero fade would make forward-lit contributions scale to zero.

```cpp
class LLPipeline::ScopedPrismNearbyLights
{
public:
    ScopedPrismNearbyLights(LLPipeline& pipeline, const LLCamera& camera)
        : mPipeline(pipeline)
    {
        mPipeline.mNearbyLights.swap(mSavedMainLights);
        mPipeline.buildPrismNearbyLights(camera, mPipeline.mNearbyLights);
    }

    ~ScopedPrismNearbyLights()
    {
        mPipeline.mNearbyLights.clear();
        mPipeline.mNearbyLights.swap(mSavedMainLights);
    }

private:
    LLPipeline& mPipeline;
    light_set_t mSavedMainLights;
};
```

Auxiliary entries can start fully visible rather than advancing a persistent
fade. Guard the ordinary `calcNearbyLights(camera)` call while
`sPrismLensRender` is true. The `ScopedPrismNearbyLights` lifetime must enclose
both `renderGeomDeferred()` and `renderDeferredLighting()`, because
`setupHWLights()` consumes the set during geometry as well as lighting setup.

If this scoped set is not implemented, the safe fallback is to reuse the last
main-view light list and document that lights visible only near the remote
camera can be absent. That fallback is safe only while the existing Prism
fade-write guards remain active. Mutating the main list is not an acceptable
fallback.

### 17.2 Projector shadow selection

Guard every source of persistent projector-priority/target mutation during
auxiliary renders:

- the `mTargetShadowSpotLight[]` reset in `renderDeferredLighting()`;
- the priority reshuffle in `setupSpotLight()`;
- both local-light-loop calls to `updateSpotLightPriority()`.

All conditions require `!sPrismLensRender`. Auxiliary shadow-target selection
is disabled, so updating persistent priority from the remote eye has no benefit;
guarding only one mutation still changes the following main frame's selection.

### 17.3 SSR and native hero sampling

Auxiliary Prism rendering occurs before the current main view has finalized a
matching scene map and temporal matrix history. Screen-space reflections would
sample previous-main data from the wrong camera. Native hero-mirror sampling
also aliases uniforms used by the Prism clip path.

Upload a generic auxiliary flag before the reflection-probe early return:

```cpp
void LLPipeline::bindReflectionProbes(LLGLSLShader& shader)
{
    static const LLStaticHashedString sPrismAux("prism_aux");
    shader.uniform1i(sPrismAux, sPrismLensRender ? 1 : 0);
    // existing probe binding follows
}
```

Guard **every** SSR and hero-probe sampling site in the class-3 reflection
shader, not just one material branch:

```glsl
uniform int prism_aux;

if (prism_aux == 0)
{
    // tapScreenSpaceReflection(...)
}

if (prism_aux == 0)
{
    // tapHeroProbe(...)
}
```

Do not advance the shared Poisson/noise offset during an auxiliary render.
Ordinary non-hero IBL probes may remain enabled.

### 17.4 Shadows

The practical first implementation reuses main-view sun and spotlight shadow
maps. A remote camera can see geometry outside the main cascades or projector
selection, so shadows can be missing or incorrect. Per-feed cascades would add
substantial CPU, GPU, and memory cost and conflict with the performance target.

Document this limitation and keep it measurable. Do not claim full Blender
camera equivalence until view-local shadows exist.

### 17.5 LOD, avatars, alpha sort, and streaming

The dedicated non-world camera ID deliberately prevents auxiliary culls from
rewriting main-view distance/LOD state. Consequently, the feed can inherit
main-camera LOD, avatar range, attachment participation, and alpha-sort
decisions. Content outside the simulator's real-camera interest set may not be
loaded at all.

Never temporarily label the feed `CAMERA_WORLD`; that would trade visible
quality gaps for corrupt main-view global state. The honest first-version
contract is:

- best for alternate angles within the already-loaded production set;
- not a remote-region surveillance system;
- remote or behind-main-camera geometry may be coarse or absent.

View-local LOD/sort state is a separate renderer project.

## 18. Adaptive performance scale and 30 FPS protection

The cheap part is fan-out compositing; the expensive part is the additional
scene view. One camera with one or sixteen visible bindings performs the same
one cull/G-buffer/lighting/copy when scheduled, followed by one to sixteen
comparatively cheap composites. It is still **not projector-cheap**: avatars,
alpha, lights, dense geometry, draw submission, and the auxiliary light scan can
approach another main-scene render.

Lowering a capture's Picture update rate saves those expensive auxiliary scene
renders. It does **not** stop its visible faces from being composited during
ordinary main frames: between publications every face continues sampling the
same retained texture. Face count therefore affects composite baseline cost, but
never multiplies camera capture count or creates independent FPS schedules.

### 18.1 Precise product contract

`Adaptive performance` protects a target; it is not an unconditional hardware
guarantee.

- For target 30, the hard frame deadline is 33.33 ms.
- If Prism-off main rendering, the currently visible fan-out composites, and
  the minimum capture tier all fit that deadline, Adaptive mode targets
  main-view render-work p95 `<= 33.33 ms`.
- If the no-aux baseline (including required visible composites) is already
  slower, suspend auxiliary captures and report `MAIN/COMPOSITE BASELINE BELOW
  TARGET`; Prism cannot repair unrelated load or promise a feed without drawing
  its screens.
- If that baseline fits but even a measured minimum-tier producer cannot fit,
  retain its output and report `AUX CAPTURE COST EXCEEDS TARGET`; this is distinct
  from blaming the main scene.
- If a known VSync/frame-limit cap is below the selected target, protect that
  lower effective target and report the selected target unavailable. Do not use
  a scene-induced low observed FPS as a cap; that must remain a baseline failure.
- Main-view protection wins over camera motion. During suspension, keep the last
  successful outputs as `HELD` rather than blanking every monitor.
- One eligible Automatic producer targets a 30 Hz picture when budget permits;
  a Target-FPS producer never requests above its validated 1-30 FPS ceiling. Two
  or three producers share the admitted global budget by max-min allocation;
  display count never divides refresh cadence.

### 18.2 Non-blocking measurement

The remainder of Sections 18.2-18.4 is the researched phase-two controller
specification. The current patch does not create timestamp queries or p95 cost
histories; it uses the explicitly described hysteretic presented-FPS fallback
summarized at the top of this document and in the Claude handoff.

Use monotonic `LLTimer` seconds—not `gFrameCount`—for cadence, retry backoff,
shrink delay, and controller hysteresis. Record separately:

- presentation interval;
- main CPU render work excluding deliberate sleep/frame-limit wait;
- main GPU time where supported;
- auxiliary CPU time around the complete capture attempt;
- auxiliary GPU time from its first GPU command through retained-output copy;
- composite CPU/GPU cost.

For the compact UI, `main render p95` is the maximum valid CPU/GPU render-work
p95 (or CPU/wall fallback); presented FPS remains a separate measurement. Do not
display `1000 / render_p95` as though it were delivered FPS, especially under
VSync or an intentional limiter.

Use paired `GL_TIMESTAMP` queries in an eight-pair ring. Poll availability a few
frames later and never synchronously request an unavailable result. Maintain a
0.5-second fast EWMA, two-second rolling p90/p95 main history, and the last 32
auxiliary samples (expire after ten seconds). Tag capture frames; learn baseline
from no-aux frames that still include the required display composites. Measure
composites separately so their contribution remains visible. Even if one
producer would capture every frame, leave at least one deliberate baseline frame
every two seconds.

For an EMPTY producer whose faces are not yet being composited, add a
conservative measured per-face composite estimate to `Bcpu/Bgpu`; first
publication must not make previously invisible display cost appear from zero.

The two-second sample is only steady-state maintenance. Mark baseline confidence
stale after timing reset, shader/GL reset, a visibility-set change, or two seconds
without a clean no-aux sample; while stale, admit no auxiliary capture until a
three-frame no-aux diagnostic burst establishes a baseline. Also start that burst
on the very next frame whenever main render-work p95 in any available domain
crosses `D`, even if the regular
baseline frame is not due. If at least two of the three diagnostic frames exceed
`D`, classify `MAIN/COMPOSITE BASELINE BELOW TARGET` and suspend immediately.
This makes the 0.5-second protection contract possible for a 36 ms baseline;
waiting for the periodic two-second sample would not.

Ignore minimized/background, teleport, shader reload, and GL reset samples and
reset estimator confidence afterward.

If GPU timestamps are unavailable for two seconds, enter
`TIMING_UNAVAILABLE` and use a concrete scalar wall-cost fallback rather than a
missing `Bgpu/Agpu` value. Let `Bwall` be p90 no-aux render-work wall time with
configured frame-limit sleep removed. For a tagged minimum-tier probe, let
`Awall_i` be the maximum of (a) auxiliary-scope CPU wall time, (b) tagged
main-render-work excess above `Bwall`, and (c) presentation overrun above the
no-aux/cap baseline across the probe frame and its next two frames. Do not admit
another attempt until that three-frame attribution window closes. With no prior
GPU samples, first establish the three-frame no-aux baseline and then use the
isolated discovery-probe rule below. Thereafter require both the ordinary CPU
test and `Bwall + Awall_i <= D`, and compute:

```text
Awall_global = max(Awall_i for i in S)
Rwall = F_effective * max(0, O - Bwall) / max(Awall_global, epsilon)
Rallowed_fallback = min(validated_total_capture_budget, 5, F_effective, Rcpu, Rwall)
```

This cannot attribute asynchronous GPU work as precisely as timestamps, so it
also forces minimum logical quality and never exceeds 5 Hz. If the wall test
fails, use zero. No path performs `glFinish` or blocks on a query.

### 18.3 Admission math

Sanitize the selected target before doing any division. Let `F_selected` be one
of 30/45/60. Let `F_cap` be a known active VSync/frame-limit ceiling, or infinity
when no such configured cap exists. Then:

```text
F_effective = min(F_selected, F_cap)
hard deadline D = 1000 / F_effective
operating deadline O = 0.93 * D
```

Thus the ordinary uncapped 30-FPS case is `D = 33.33 ms`, `O = 31.00 ms`. Every
admission, emergency, recovery, and upgrade threshold derives from the same
`F_effective`; the UI still shows selected versus effective target separately.
All dwell logic remains time-based. Never infer `F_cap` from slow observed scene
frames, or the controller would redefine failure as success.

Let `Bcpu/Bgpu` be confident no-capture p90 main cost. Let
`Acpu_i/Agpu_i` be the nonexpired p90 cost of eligible capture `i` at its
candidate logical-resolution tier. Every capture tier is first checked
individually:

```text
Bcpu + Acpu_i <= D
Bgpu + Agpu_i <= D
```

Only individually safe captures enter steady-state set `S`. To make one global
deadline deterministic for heterogeneous captures, budget against the worst
admitted member, not whichever candidate happened to be visited last:

```text
Aglobal_cpu = max(Acpu_i for i in S)
Aglobal_gpu = max(Agpu_i for i in S)
```

When timestamps are unavailable, define `S` by the CPU and wall predicates from
Section 18.2 and substitute `Awall_global/Rwall` for the GPU predicate/rate; do
not evaluate an absent GPU variable.

For `F_effective`, sustainable global cadence is:

```text
Rcpu = F_effective * max(0, O - Bcpu) / max(Aglobal_cpu, epsilon)
Rgpu = F_effective * max(0, O - Bgpu) / max(Aglobal_gpu, epsilon)

Rallowed = min(validated_total_capture_budget, 30, F_effective, Rcpu, Rgpu)
```

If `S` is empty, steady-state `Rallowed` is zero and only the bounded diagnostic
probe path below may run. A saved Manual `Every Frame` sentinel is validated as
30 Hz for this Adaptive formula.

Quantize the global admitted budget downward to `30, 20, 15, 10, 5, 0`
attempts/second. Water filling operates only on the current steady-eligible set:

```text
W(now) = { i in S | captureEligible(i, now) }
```

Thus source-offline, unprepared, offscreen, and retry-backed-off producers hold no
share. Recompute when a producer enters/leaves `W`, including when retry expires;
re-entry receives a rebased deadline, never accumulated service credit. For each
capture in `W` define its requested ceiling:

```text
d_i = 30                         for Automatic
d_i = validated target_output_fps for Target FPS
```

Allocate per-capture entitlements by deterministic max-min water filling:

```text
0 <= e_i <= d_i
sum(e_i) <= Rallowed
```

Divide the remaining budget equally among unsatisfied captures, freeze every
capture whose requested ceiling is reached, and redistribute the remainder until
all captures are satisfied or no budget remains. With `Rallowed = 30`:

```text
requests 30/30/30 -> entitlements 10/10/10
requests 5/30     -> entitlements 5/25
requests 5/10/30  -> entitlements 5/10/15
requests 5/10     -> entitlements 5/10; unused budget remains unused
```

This avoids wasting budget on low-rate creative choices while preserving equal
access among unsatisfied producers. Binding count supplies no demand and no vote.
Expose `e_i` separately from measured successful-publication rate; render
failures can make observed FPS lower. Actual capture-frame deltas remain an
additional veto because asynchronous GPU work is not attributed perfectly.

An unmeasured rung is never treated as free. From the nearest nonexpired sample
with logical pixel-area ratio `q = Pnew / Pold`, predict
`Acpu = 1.10 * Acpu_old` and
`Agpu = 1.25 * Agpu_old * max(1, q)` until a probe measures the rung. This
intentionally assumes no saving when scaling down. With no sample, exclude the
capture from steady-state `S` and use a minimum-tier probe whose longest logical
axis is at most 256. Invalidate that capture's cost model on source rebind,
optics/aspect edit, scene replacement, shader/GL reset, or a material demand-area
change; expire it after ten seconds.

Probing has its own monotonic wall-clock deadline and therefore still runs when
the admitted global rate is zero. If the confident no-aux baseline itself fits
`D` in every available timing domain but an unmeasured/rejected capture cannot
pass predicted admission, allow one **isolated discovery probe** at the
minimum-resolution tier. This necessarily accepts the possibility of one slow
learning frame—there is no way to learn a new view's render cost without drawing
it—but it does not weaken the stabilized p95 contract. Space discovery probes by
at least 2.1 seconds globally, choose pending producers oldest-first, and with the
three-producer cap give each one a turn within 6.3 seconds. If the baseline does
not itself fit `D`, run no probe and use the baseline-failure state.

Probes obey the one-attempt-per-frame limit and normal allocation backoff. A
probe consumes the frame's sole auxiliary opportunity; if the ordinary global
deadline is already due, advance/discard it as though an attempt occurred so the
probe cannot be followed by a catch-up burst. In the no-timestamp fallback, wait
for the two following attribution frames before classifying its wall cost. A
measured safe lower tier joins `S`; an unsafe one remains held/paused until cost
invalidation or new baseline headroom. This discovers the important moderate-
headroom case where a high tier is unsafe but the minimum tier actually fits,
without repeatedly gambling ordinary frames.

The controller evaluates candidates before scheduler selection. A too-expensive
oldest capture must not block a smaller admissible sibling; the rejected capture
reports `PAUSED` with its admission reason and remains oldest for a later
lower-tier probe. Once a safe reduced tier is actually updating, it becomes
`THROTTLED`.

### 18.4 Scale, cadence, and hysteresis

User quality (0.25–2.0) is a global ceiling. Each capture has its own effective
runtime scale so an expensive angle can downshift without needlessly blurring a
cheap sibling; the global snapshot reports the min/max applied range. Adaptive
scale never rewrites the saved setting. Resolution rungs are
`2.0, 1.5, 1.0, 0.75, 0.5, 0.35, 0.25`, starting at the highest rung no greater
than the user's ceiling. Global admitted-budget rungs are
`30, 20, 15, 10, 5, 0` attempts/second; per-capture entitlements are the
possibly fractional results of water filling against each requested ceiling.

Degrade in this order:

1. lower that capture's logical resolution while attempting to retain its
   requested Picture update rate (never above 30 FPS in Adaptive);
2. lower admitted global budget and recompute all producer entitlements without
   exceeding any per-capture request;
3. suspend capture and retain outputs.

Do not infer CPU/draw-bound behavior from nominal scale rungs that map to the
same clamped/bucketed logical extent. Count only a transition whose actual
logical pixel area falls by at least 20 percent. For each such transition compute
`expected_drop = 1 - Pnew/Pold`, `observed_drop = 1 - Gnew/Gold`, and
`efficiency = observed_drop/expected_drop`. Only after two measured transitions
with `efficiency < 0.25` may the controller mark the workload CPU/draw-bound and
skip remaining ineffective resolution rungs. Recovery favors motion: restore
cadence first, resolution second.

Evaluate four times/second. Downshift after predicted cost exceeds the operating
deadline or main render-work p95 exceeds the hard deadline continuously for 0.5 seconds;
allow at most one ordinary downshift/second. Three consecutive render-work
frames above `1.2 * D` (40 ms at 30 FPS) trigger immediate one-second capture
suspension. Resume with one minimum-tier probe only after no-capture baseline
stays below `0.87 * D` (29 ms at 30 FPS) for two seconds.

Upgrade only when main render-work p95 remains below `0.81 * D` (27 ms at 30 FPS) and the
proposed next rung is predicted below `0.87 * D` for five seconds. Restore one
rung per five seconds, with no upgrade for five seconds after any downshift.
These asymmetric thresholds prevent oscillation near the target.

Numeric cadence uses one phase-preserving global deadline plus one
`mNextSteadyDueTime` per capture, and a steady attempt requires both. Manual
Every Frame instead requires its one current-frame token plus the producer
deadline. After an attempt advance every applicable numeric deadline; Every
Frame consumes the token and advances only the producer deadline. If more than
one period was missed, discard the backlog and restart from now. Never issue
catch-up captures. Allocation failure consumes its opportunity and backs off
that producer for 0.5, 1, 2, 4, then at most 8 seconds; success resets backoff.

The global deadline has the same explicit transition rule as each producer. For
a new positive numeric budget `Rnew`:

```text
next_global_due = max(now, last_global_attempt + 1 / Rnew)
```

Changing to zero disables the numeric global deadline. Re-entering a positive
rate uses the formula above and grants no saved credit. A probe that runs while a
numeric global deadline is already due advances/discards that deadline exactly
once. Adaptive/Manual transitions rebase both global and producer deadlines;
neither direction can stall on an old low-rate deadline or burst from backlog.

### 18.5 User-facing performance scale

Expose the user quality ceiling and the controller's read-only **applied scale
range** side by side. Add these global settings:

```text
PrismAdaptivePerformance       true
PrismProtectedMainFPS          30       // choices 30, 45, 60
PrismCaptureRefreshCeilingHz   30       // total attempts/s: 5,10,15,20,30; 0 = Manual Every Frame
PrismLensResolutionScale       1.0      // existing key, now shared; 0.25–2.0
```

Sanitize the typed saved values before any deadline math: accept protected FPS
only from `{30, 45, 60}` and otherwise use 30; accept refresh only from
`{0, 5, 10, 15, 20, 30}` and otherwise use 30; if quality is non-finite use 1.0,
then clamp it to `[0.25, 2.0]`. A detected active presentation cap must likewise
be finite and positive before it can become `F_cap`; otherwise treat it as no
known cap. Emit one settings warning, not per-frame spam. Target zero or an
extreme hand-edited integer must never reach `1000 / F_effective`.

Keep the existing `PrismLensResolutionScale` key as the shared quality ceiling
for both Lens and Camera Feed. Reusing it avoids an ambiguous one-time migration
and preserves the user's existing preference. The UI label is simply `Quality
ceiling`; the legacy internal key need not be exposed. Runtime readouts are not
saved:

```text
Applied capture scales    0.35–0.75x
Admitted global attempts  20 Hz
Observed global attempts  19.7 Hz
Main render p95           31.4 ms
Presented                 31.8 FPS
State                      PROTECTING 30 FPS
```

The controller may move each producer only downward from the selected ceiling
without user input. Label the global control `Total capture budget`, never
`Camera FPS` or `Capture refresh ceiling`: it caps aggregate auxiliary attempts
and is not any one producer's publication rate. Each capture row separately
reports requested, entitled, and observed Picture FPS because none is
interchangeable with the global values above.

Manual mode disables the 30-FPS protection. A numeric Total capture budget uses
the same water filling and positive global/per-producer deadline rules, without
Adaptive cost admission; its `W` is every currently `captureEligible()` producer
rather than the Adaptive-safe subset `S`.

Its global menu additionally exposes `Every Frame`
(`PrismCaptureRefreshCeilingHz = 0`). This is a distinct executable scheduler
mode, not a zero-Hz rate:

- every eligible main frame contributes exactly one auxiliary-attempt token;
  there is no numeric global deadline and no division by the zero sentinel;
- maintain `Rframe`, a finite rolling rate of actual token-bearing main-frame
  opportunities (use the last valid rate, or 30 during initial learning, and
  clamp invalid values before math);
- water-fill capacity `Rframe` over `W`, with Automatic demand `d_i = Rframe`
  and Target demand `d_i = target_output_fps`;
- retain the resulting per-producer deadlines and choose the most-overdue due
  producer for each token. A frame with no due demand simply leaves its token
  unused;
- recompute at most four times/second when `Rframe` or `W` materially changes and
  rebase deadlines without deficit, credit, or catch-up;
- show `Every Frame (~Rframe opportunities/s)` globally and continue to show each
  producer's Entitled/Observed rates. Physical tokens, not the estimate, remain
  the hard one-attempt-per-main-frame limit.

This gives a 30-FPS main view with Automatic + Target 30 approximately 15/15,
while a 60-FPS main view gives approximately 30/30; Automatic alone may exceed
30 only in this explicitly unprotected Manual mode. Manual still cannot guarantee
a target above physical main-frame availability or a producer's fair share.
Transitioning into or out of Every Frame clears only cadence phase, not retained
output or cost history.

While Adaptive is on, keep a saved zero visible as the selected but disabled row
`Every Frame (Manual only; Adaptive uses 30 attempts/s)` and show the effective
30-attempt/s budget. This avoids a setting-bound combo with no selected item.
Validation preserves the saved Manual choice while clamping only its Adaptive
interpretation to 30.

### 18.6 Hard resource bounds

- three capture producers total across Lens and Camera Feed;
- sixteen configured display bindings initially (raise only after weakest-target
  composite benchmarks);
- one auxiliary attempt per main frame;
- 30 auxiliary attempts/second maximum in protected Adaptive mode;
- logical axes 64–1024 pixels and no more than 1,048,576 logical pixels;
- three retained fixed-capacity `GL_RGBA16F` outputs, three lazily allocated
  exact-size deferred scratch packs, and matching exact-size water targets;
- eight outstanding GPU timestamp pairs;
- no `Unlimited` rate inside Adaptive mode. A clearly labeled Manual override
  may render up to one capture/main frame but disables the 30-FPS protection.

With all three captures simultaneously grown to 1024 on each axis, worst-case
target storage is roughly 183-207 MiB before driver/FBO overhead (the range
depends on deferred-light/emissive formats). Typical use is lower because packs
are lazy and exact-size. Display bindings own no render targets. This bounded
VRAM increase is the correctness/performance tradeoff that prevents alternating
mixed-size captures from reallocating one shared pack or sampling stale texels.

## 19. Director-scene persistence

Definitions are viewer-local and must never write prim/material data or send
object updates. For a complete machinima feature, serialize them with Director
scene files rather than hiding UUID arrays in `settings.xml`.

Suggested LLSD:

```json
{
  "version": 3,
  "prism_captures": [
    {
      "capture_id": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
      "mode": "camera_feed",
      "camera_id": "22222222-2222-4222-8222-222222222222",
      "fov_mode": "fixed",
      "fixed_vertical_fov_radians": 1.0471976,
      "near_clip": 0.05,
      "far_clip": 256.0,
      "local_eye_offset": [0.0, 0.0, 0.0],
      "output_aspect": 1.7777778,
      "output_rate_mode": "target_fps",
      "target_output_fps": 24.0
    }
  ],
  "prism_displays": [
    {
      "binding_id": "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
      "capture_id": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
      "display_id": "11111111-1111-4111-8111-111111111111",
      "display_te": 2,
      "fit": "fit",
      "anchor": [0.5, 0.5],
      "bar_color_linear": [0.0, 0.0, 0.0]
    }
  ]
}
```

Persistent IDs—not runtime slots/generations—join the arrays. Both capture modes
require the common `capture_id`, `mode`, `output_rate_mode`, and
`target_output_fps` fields. A `surface_lens` omits Camera-Feed source/optics/aspect
fields and must have exactly one referencing display. A `camera_feed` requires
those optics/aspect fields, may have a null `camera_id`, and may have zero or many
displays within the global cap. Runtime slots and fresh generations are assigned
only after successful load.

Loading rules:

- for version 3, require both `prism_captures` and `prism_displays` to be present
  and array-typed; a missing member is malformed rather than an implied clear;
- parse and validate both complete arrays and their references before replacing
  the live registry;
- maximum three captures and sixteen displays;
- nonnull unique capture/binding IDs and resolvable capture references;
- nonnull display-object UUIDs (`camera_id` is the only intentionally nullable
  object reference);
- unique display `(UUID, TE)` identity across every producer;
- require `display_te` to be an integral LLSD value with
  `0 <= display_te < LLTEContents::MAX_TES`;
- known enum strings only;
- require `output_rate_mode` to be `automatic` or `target_fps`, and require
  `target_output_fps` to be finite in `[1, 30]` even in Automatic mode so the
  previous target is retained for a later mode switch;
- finite bounded optics and aspect; each anchor component and linear bar-color
  RGB component must be finite in the inclusive range `[0, 1]`;
- exactly one display for Lens; zero through sixteen for Camera Feed;
- for Camera Feed, reapply the runtime invariant that its nonnull source-object
  UUID differs from every referencing display-object UUID;
- unresolved world objects are valid offline bindings;
- version-3 explicitly empty capture/display arrays clear the registry;
- scene load never changes the master render toggle or sends object messages.

Bump Director `SCENE_VERSION` from 1 to 3. Version-1 scenes have no Prism payload
and preserve the live registry. Production never emitted a version-2 Prism
schema, so do not invent a `prism_views` migration without a real field contract:
version 2 is unsupported, preserves the live registry, and emits one warning.
Malformed versions and unknown future versions behave the same way. Version-3
data is applied through `applySceneData()` only after a complete
temporary-registry parse and reservation of fresh process-lifetime generations;
failure leaves configuration, outputs, GPU targets, and open-editor handles
untouched. Commit capture deletion/output release and binding replacement as one
transaction. `sceneData()` performs the inverse mapping.

Add `PrismLensZoom` to `sceneSettingsList()` so Lens appearance is reproducible.
Per-capture output-rate mode/target are scene-level creative intent and are
serialized in `prism_captures`; they are not a new global setting. Quality
ceiling, Adaptive toggle, protected main-view FPS target, and Total capture
budget remain global performance preferences; master enable/debug remain runtime
controls and are not activated by scene load. Effective scale/entitlement,
observed publication rate, deadlines, and timing histories are runtime-only and
never serialized.

Master off retains definitions but releases GPU resources. Floater close and
teleport retain definitions. Logout/account switch clears the live registry and
GPU/runtime state near the existing session-only cleanup in `llappviewer.cpp`;
saved Director scene files remain intact.

When master-off releases retained targets, iterate the three captures once. Set
each output to `EMPTY` and activity to `PAUSED` in the same operation that clears
`mHasOutput`, and advance runtime revision without changing capture/display
configuration. On re-enable, visible demanded producers become `WAITING` until
they republish and producers without visible consumers become `IDLE`. Bindings
never own or release GL targets.

## 20. Required file-level integration map

This section records the research target, not an exact post-implementation diff.
In particular, the current patch keeps the lightweight controller inside
`llprismlens.cpp`, does not add `llprismperformance.*`, and reuses the
pre-existing `llviewerdisplay.cpp` Prism hook. See
`doc/PRISM_CAMERA_FEED_CLAUDE_HANDOFF.md` for the authoritative changed-file
inventory.

Primary implementation files:

- `indra/newview/llprismlens.h`
  - add capture/display handles, snapshots, fit/settings/runtime types, separate
    caps, capture rate mode/target and setter, selection eligibility, revisions,
    and atomic scene-data APIs;
- `indra/newview/llprismlens.cpp`
  - fixed three-capture and sixteen-binding stores with generation-safe IDs;
  - binding-first preparation and maximum-demand aggregation;
  - resolve source render transform;
  - add symmetric camera branch;
  - disable face clipping for feeds;
  - source-eye water state;
  - typed display/source failure without `rejectSlot()` deletion;
  - wall-clock global/per-producer deadlines, max-min rate allocation, bounded-
    fair selection, and invalidation/cut handling;
  - canonical Camera Feed orientation, capture-time retained Lens orientation,
    plus per-binding Fit/Fill/Stretch;
  - transactional retained-output growth that preserves the old valid target on
    allocation failure;
- new `llprismperformance.h` / `.cpp`
  - nonblocking CPU/GPU histories, deadline admission, adaptive scale/cadence,
    phase-preserving timing, hysteresis, status, and reset hooks;
- `indra/newview/pipeline.h` / `pipeline.cpp`
  - keep three output targets but enlarge composite-state storage to sixteen;
  - treat composite index and capture texture slot as different values;
  - group composites by capture texture;
  - three exact-size capture scratch packs plus exact-size water depth/color and
    exclusion targets, with post-bind viewport/scissor reapplication and matching
    screen-size uniforms through every nested Prism target bind;
  - eight-pair nonblocking GPU timestamp ring;
  - mode-aware destination versus water clip selection;
  - scoped auxiliary nearby-light set;
  - all projector priority/shadow mutation guards;
  - generic `prism_aux` upload/noise guard;
- `indra/newview/llsettingsvo.cpp`
  - active-mode clip behavior and source-eye water/fog;
- `indra/newview/app_settings/shaders/class3/deferred/reflectionProbeF.glsl`
  - guard every SSR and hero sampling branch;
- `indra/newview/app_settings/shaders/class1/deferred/prismLensF.glsl`
  - true letterbox test/bar color and retained logical-region transform;
- `indra/newview/llfloaterdirector.h` / `.cpp`
  - compact summary/Manage control and scene serialization/deserialization;
- new `indra/newview/llfloaterprismmanager.h` / `.cpp` and
  `skins/default/xui/en/floater_prism_manager.xml`
  - capture and display lists, safe cascade removal, generation-bound editors,
    optics/aspect, Automatic/Target Picture FPS, fit/anchor/bar settings, and live
    requested/entitled/observed adaptive readout;
  - bind to persistent ID plus generation; on stale identity clear the embedded
    detail selection, disable mutations, refresh from snapshot, and keep the
    manager open;
  - use fixed summary/list/footer regions plus tab-local fixed-document scrollers
    as specified in Section 5.1; scroll focus/errors into view;
- `indra/newview/llviewerdisplay.cpp`
  - provide clean no-capture/main timing boundaries and invoke admission before
    the optional producer render;
- `indra/newview/CMakeLists.txt`
  - add manager/performance `.cpp` and `.h` files to the viewer target; skins XUI
    is already recursively collected and needs no explicit source entry;
- `indra/newview/llviewerfloaterreg.cpp`
  - register the new floater alongside Director so `LLFloaterReg` can open it;
- `indra/newview/skins/default/xui/en/floater_director.xml`
  - compact Prism summary/Manage row without overlapping `camera_flow_scroll`;
- `indra/newview/llappviewer.cpp`
  - clear live session registry/runtime state during account logout cleanup;
- `indra/newview/app_settings/settings.xml`
  - Adaptive toggle, protected main-view FPS target, quality ceiling, Total
    capture budget, and only global defaults—not per-capture rate intent or
    capture/display UUID arrays;
- user guide and Claude handoff documents.
- `doc/MACHINIMA_STANDALONE_FLOATER_SCROLL_AUDIT.md`
  - apply its common scrolling contract to the new manager and keep its unrelated
    P0/P1/P2 repairs as a separately reviewable UI change set.

Files that should not be touched for this feature include hero-probe manager,
reflection-map ownership, probe network data, and mirror material flags.

## 21. Adversarial review: ways this approach can be wrong

### Must-fix before build

1. **Destination clip leakage** — If feed mode inherits Lens's user/fragment
   plane, seemingly random remote geometry disappears.
2. **Wrong axis convention** — Using the viewer camera's local `+X` convention
   directly on the marker instead of the selected `-Z/+Y` product convention
   makes every camera point sideways.
3. **Raw transform use** — `getPosition()` / `getRotation()` can be parent-local
   and miss rendered interpolation. Use render-space accessors.
4. **Network FOV mutation** — Calling `setView()` sends `AgentFOV`. Only the
   no-broadcast setter is allowed.
5. **Display-dependent feed aspect** — Deriving projection from any binding,
   projected AABB, or scratch shape makes all screens reframe when that binding
   changes. Aspect belongs to the capture.
6. **False self-hide guarantee** — A `stateSort(drawable)` early return misses
   cached group submissions. Use a transparent marker/forward eye offset in v1,
   or implement complete submission-level filtering later.
7. **Source/display same object** — Feedback semantics are ambiguous even with
   recursion suppression. Reject that binding.
8. **Conflated capture/display state** — One offline display must not suppress a
   shared output or prevent healthy siblings from compositing it.
9. **Main light-list corruption** — Running ordinary `calcNearbyLights()` from
   the remote eye mutates main state. The aux builder must ignore global nearby
   flags, set full fade, span both render phases, and restore by scope.
10. **Spot-shadow target mutation** — Guard target reset, priority reshuffle,
    and both `updateSpotLightPriority()` calls.
11. **Invalid temporal SSR** — Disable every SSR branch and shared noise advance.
12. **Hero uniform alias/feedback** — Suppress hero taps and all Prism
    composites during aux.
13. **Wrong water eye** — Camera feeds use the source eye; Lens uses the real
    main eye.
14. **Publishing before restoration** — Mark an output valid only after the
    complete render-state scope exits successfully.
15. **Wrong invalidation ownership** — Source/FOV/clip/aspect edits suppress the
    shared output; Fit/anchor/display edits recompute only that consumer.
16. **Urgent-capture starvation** — A camera that cuts every frame must not
    outrank other producers forever. Do not grant cut priority; among captures
    whose rate deadlines are due, schedule the most overdue with oldest attempt
    and rotating slot as bounded tie-breakers.
17. **CPU/GL far mismatch** — Validate against `MIN_FAR_PLANE` and use one
    effective far value for `LLCamera` and `glm::frustum`.
18. **Lost water clipping** — Feed mode disables only the destination plane; it
    must fall through to normal source-camera water clipping.
19. **Failure type inference** — Missing/dead/rebuilding source is transient;
    invalid type/configuration is invalid. Return a typed result.
20. **Configuration erased by runtime loss** — Display/source disappearance must
    never invoke the old slot-deleting rejection path.
21. **Frozen UI status** — Director watches runtime as well as configuration
    revision and exposes reason/effective optics/output age.
22. **Fake resolution scaling** — A smaller output does not save GPU work if the
    old full scratch viewport/uniform dimensions are still rendered. Logical
    extent must control actual shaded pixels.
23. **Offscreen is not offline** — Screen footprint controls scheduling, not
    binding health; source health still resolves while the display is hidden.
24. **Released texture reported current** — Master teardown updates
    `mHasOutput`, output/activity state, and runtime revision atomically.
25. **Stale editor writes reused storage** — Every capture/display mutation
    verifies persistent ID and runtime generation.
26. **Cap conflation** — Three outputs and sixteen composites are different
    bounds. Never size composite arrays with `MAX_CAPTURES` or index an output
    with a binding index.
27. **Fit edge smear** — Clamping out-of-range Fit UV repeats edge texels. Test
    before sampling and emit the configured bar color.
28. **Binding-count priority** — Ten monitors must not give one camera ten
    scheduler votes. Aggregate maximum pixel demand; never sum demand or count.
29. **Binding owns GL target** — Bindings never own or directly release GL
    targets. Removing one of several screens leaves its capture and siblings
    untouched. A transition to zero consumers may release that capture's owned
    scratch/output while preserving its definition; the next binding republishes
    lazily.
30. **Recursive fan-out** — All Prism composites remain suppressed during every
    auxiliary capture; do not special-case screens sharing the active capture.
31. **Frame-count cadence** — Scheduling and retry/hysteresis use monotonic
    seconds, or refresh rate changes with main FPS and stalls create bursts.
32. **Blocking GPU timing** — Never wait for a query result. Delayed timestamp
    polling and conservative fallback are mandatory.
33. **False 30-FPS guarantee** — If the no-capture baseline misses 33.33 ms,
    suspend and report it rather than blaming or endlessly degrading Prism.
34. **Controller oscillation** — Fast downshift, slow recovery, dwell times, and
    separate emergency suspension must survive noisy 29–34 ms workloads.
35. **Catch-up burst** — Missed cadence periods are discarded; never render
    multiple auxiliary views to repay backlog.
36. **Unusable generation contract** — Creation returns full handles and the
    bounded snapshot enumerates current handles; a UUID-only API cannot safely
    drive the manager.
37. **Generation resurrection** — Process-lifetime `U64` generations never reset
    on scene load, so reloading the same persistent IDs cannot revive stale UI.
38. **Status precedence hides faults** — Capture Health/Output/Activity and
    Display Health/Visibility are separate columns and may be shown together.
39. **Vector operator trap** — `LLVector2 * LLVector2` is not componentwise and
    vector division is unavailable; Fit/Fill examples use explicit components.
40. **Shader wired to nowhere** — Patch the loaded `class1/deferred` shader and
    its existing `prism_uv` interface, not an `interface` path/undefined varying.
41. **FBO bind defeats scaling** — Every direct and nested Prism target bind
    reapplies the logical extent after `bindTarget()` resets the viewport;
    frustum and uniforms use that same extent.
42. **Held Lens flips sides** — Retain Lens orientation at publication; never
    interpret an old texture from the aperture's current side.
43. **Order-dependent global budget** — Admission checks each producer and uses
    the worst cost in the deterministic admitted set, not the last candidate.
44. **Permanent unknown-cost throttle** — Cost histories expire/invalidate and a
    bounded isolated wall-clock probe runs even when steady global rate is zero.
45. **False CPU-bound plateau** — Only measured logical pixel-area reductions
    count, and observed GPU reduction is normalized by expected area reduction.
46. **Destructive target growth** — Allocate/validate/swap before releasing the
    valid retained output; failure cannot blank sibling displays.
47. **Late baseline discovery** — Deadline crossings force an immediate no-aux
    diagnostic burst; the ordinary two-second sample is not the alarm path.
48. **Imaginary v2 migration** — Unsupported/unshipped version 2 and unknown
    future schemas preserve live state and warn; version 3 requires both arrays.
49. **Below-target cap paradox** — Selected/effective target are separate and all
    deadlines derive from the same sanitized `F_effective`, not selected 30 with
    an effective 20-FPS cadence.
50. **Missing GPU time equals zero** — Timestamp loss switches to an explicit
    three-frame-attributed wall-cost domain, minimum quality, and at most 5 Hz.
51. **Probe headroom dead zone** — A bounded isolated minimum-tier probe can
    discover an actually safe low rung whenever baseline itself fits the target.
52. **Malformed preference division** — Sanitize target/rate sets and finite
    quality before calculating deadlines or applying UI-bound settings.
53. **Unimplementable persistence bounds** — Version 3 rejects null display IDs
    and defines anchor/bar components as finite `[0,1]` values.
54. **Per-face FPS destroys fan-out** — Independent display rates either do
    nothing because every face samples one retained revision, or require
    per-binding history/copies and duplicate work. Rate is capture-owned and
    display rows are read-only.
55. **Target presented as a guarantee** — `Target FPS` is a requested ceiling;
    global admission, main-frame availability, and failures may lower Entitled or
    Observed. Show all three values.
56. **Equal split wastes capped demand** — Plain `Rallowed / count` strands
    budget beside a 5-FPS request. Use max-min water filling and redistribute
    only to unsatisfied captures.
57. **Rate edit creates backlog** — Changing Automatic/Target or numeric FPS
    must recompute one next deadline with no credit, catch-up, output blank, or
    target reallocation.
58. **Global budget mislabeled Camera FPS** — The total auxiliary-attempt budget
    is not a producer's publication rate. Keep its label, persistence, and
    readouts distinct from per-capture Picture FPS.
59. **Imaginary manager implementation** — Do not claim the brief's floater is
    built until class/header, XUI, CMake membership, registry entry, Director
    callback, and all control wiring exist.
60. **Scrolled document shrinks instead of scrolling** — A settings document
    following bottom/vertical `all` can collapse with its viewport. It needs a
    definite natural height and left/top/right follows inside a page-local
    scroller.
61. **One giant wheel owner** — Wrapping lists and every tab in one outer scroller
    steals wheel behavior and makes sticky actions move away. Lists retain their
    own scrollbars; only variable-height form documents receive page scrollers.
62. **Orphaned Surface Lens** — Removing its only display while retaining the
    producer violates the aperture invariant. Reject display-only removal and use
    the confirmed capture cascade.
63. **Keyboard focus below the viewport** — `LLScrollContainer` does not reveal
    focused descendants automatically. Every document control needs a focus
    callback, document-coordinate conversion, and `scrollToShowRect()`.
64. **False scrollbar gutter** — `reserve_scroll_corner="false"` does not reserve
    vertical-bar width. Give the direct document an explicit inset and test that
    no right-edge control or horizontal bar appears.
65. **Camera-only capture editor** — Rate settings apply to both producer modes.
    The Capture tab always shows the common rate group and conditionally shows
    Camera Feed optics or Lens-only controls.

### Known, explicitly accepted first-version limits

- main-view shadow maps are reused;
- remote LOD, avatars, alpha sorting, and simulator interest are not independent;
- transparent objects in front of the display can be ordered incorrectly by the
  existing late composite;
- camera feeds seen by camera feeds show base materials because recursion is
  suppressed;
- no depth of field, motion blur history, lens distortion, or orthographic mode;
- no automatic removal of visible camera-marker geometry from its own feed;
- a fixed combined three-capture and sixteen-display cap;
- Adaptive mode may lower picture cadence/resolution or hold outputs; 30 Hz is
  conditional on measured main-view headroom;
- source/display UUIDs do not survive re-rezzing their objects.

These limits are preferable to silently mutating the main camera or renderer
state. They should appear in the user guide rather than being discovered in an
in-world shoot.

## 22. Acceptance and adversarial test matrix

### Camera semantics

- Put asymmetric text in front of the source. Verify correct left/right and up.
- Rotate the marker in 90-degree increments around local axes.
- Confirm local `-Z` is the look direction and local `+Y` is image up.
- Move and rotate source and display independently.
- Test fixed FOV at 5°, 60°, and 175°.
- Test Follow Projector FOV and reject it on a non-projector source.
- Test near/far clipping and invalid near >= far input.
- Test a non-HUD attachment/vehicle-mounted camera.

### Binding and lifetime

- Reject no selection, multi-selection, HUD, rigged/animesh source, and source
  equal to display object.
- Bind/rebind a source and prove all sibling screens suppress the old output
  until the new shared capture publishes.
- Move source outside the local object list and back; separate columns transition
  `READY/CURRENT/LIVE -> SOURCE_OFFLINE/HELD/WAITING -> READY/CURRENT/LIVE`
  without losing its UUID.
- Bind 1 then 16 faces to one camera; every screen samples the same output
  generation and only one capture is scheduled.
- Delete/unload one display; retain its binding as offline and prove healthy
  siblings and capture state are unchanged.
- Remove/reassign one display and prove it never releases the parent output.
- For a Surface Lens, prove Add/Remove Display and Camera Feed mapping controls
  are disabled, direct `removeDisplay()` is rejected with no mutation, and only
  confirmed Remove Capture atomically removes the capture/binding pair.
- Reject a 17th display, duplicate `(UUID, TE)`, fourth capture, Lens with zero
  or two bindings, and source object used as its own display.
- Load version 1, unsupported version 2, version 3, and an unknown future version.
  Version 1/2/future preserve live state as specified; version 3 rejects a
  missing array, non-integral/out-of-range TE, source equal to display, duplicate
  IDs/displays, malformed fit/aspect, and bad references atomically. An explicit
  pair of empty version-3 arrays clears.
- Leave capture/display editors open, remove/re-add or replace the scene, and
  prove stale ID/generation handles cannot mutate new records, including loading
  the same persistent UUIDs twice in one process. The Prism Manager remains open,
  clears its stale detail selection, disables mutations, and recovers from a
  fresh snapshot.
- Verify every creation call returns a usable generation, snapshots enumerate
  all 3/16 records, and all four non-mutating selection-status APIs exactly match
  the corresponding action result without changing revisions.

### Three-capture fan-out behavior

- Lens / Feed / Feed and Feed / Feed / Feed combinations.
- Assert auxiliary scene-render count `<= 1` per main frame.
- Assert composite count `<= 16` and every composite capture index `< 3`.
- Compare one camera with 1, 8, and 16 bindings: auxiliary render count and
  retained-target count remain identical; only composite cost changes.
- Mix 16:9, 4:3, square, portrait, tiny, and large faces. Adding/removing a face
  never changes camera framing.
- Verify centered and corner anchors, Fit bars without edge smear, Fill crop,
  Stretch, back-side orientation, and bar color in linear space.
- Move a healthy display offscreen/below the area threshold: health stays ready,
  source status still updates, and that binding gives no demand/vote. All
  bindings offscreen make the producer `IDLE`, not offline/paused.
- Force retained-target growth failure while a valid output exists: the old
  target/dimensions/revision remain intact, siblings keep displaying it, and the
  producer backs off without starving the other two.
- Publish a Lens image from one side, cross the aperture without another capture,
  and prove the held image keeps its publication-time orientation.
- Move one source by more than the cut threshold every frame; the two steady
  captures retain a bounded refresh interval.

### Picture rate and Prism Manager

- Run one camera at 24 FPS first with one face and then sixteen. Assert one
  auxiliary render per publication, identical output revision/timestamp on every
  face, and no display-owned deadline or scheduler vote.
- With `Rallowed = 30`, assert exact max-min allocations `30/30/30 -> 10/10/10`,
  `5/30 -> 5/25`, `5/10/30 -> 5/10/15`, and `5/10 -> 5/10` with no invented work.
- Change face count while holding visibility and scene cost constant. Entitlement
  changes only if measured composite baseline indirectly changes admission.
- In Adaptive mode reduce resolution first, then entitlement, and never exceed a
  Target-FPS request. Verify recovery restores cadence toward each request before
  increasing resolution.
- Exercise Automatic/Target changes and visibility transitions; assert no
  catch-up burst, blanked retained output, target reallocation, or valid-cost
  history reset.
- Run 23.976, 24, and 29.97 targets long enough to verify monotonic deadline
  averages and only expected quantization jitter from the main-frame rate.
- Force failed attempts; each consumes cadence while Observed may fall below
  Entitled and the frozen shared output remains coherent.
- Round-trip Automatic and Target scene data. Atomically reject missing/unknown
  mode, NaN/infinity, zero, and target values above 30 without changing live
  records or outputs. Repeat missing/invalid common rate fields on both Camera
  Feed and Surface Lens, including a missing target while mode is Automatic.
- Put one producer into 8-second retry backoff and verify it immediately
  relinquishes its water-fill share to eligible siblings; on retry expiry it
  regains a recomputed entitlement with no saved deadline credit or burst.
- Exercise numeric-budget changes, zero/positive transitions, and
  Adaptive-to-Manual transitions. Rebase global and producer deadlines without a
  stall, divide-by-zero, double attempt, or catch-up.
- Exercise Manual Every Frame at stable 30, 60, and 120 main FPS with Automatic,
  Target 5, Target 30, and mixed producers. Assert one token/attempt maximum per
  main frame, max-min long-run service, unused tokens when no producer is due,
  and truthful `Rframe`/Entitled readouts.
- Prove a Display row is read-only and always identifies the source capture whose
  rate it inherits.
- Select both a Lens and Camera Feed capture. The common Picture update-rate
  group remains available in the same Capture tab while only the correct
  mode-specific controls appear.
- Open the manager at default, minimum, and a saved minimum rectangle at 100,
  125, 150, and 200 percent UI scale. Reach every control with wheel, scrollbar,
  and keyboard; selection/error reveals must scroll focus into view while capture
  and display lists retain their own wheel behavior.

### Render correctness and isolation

- Test a dedicated transparent marker and `Place eye in front` on an opaque
  static camera prop. Confirm no self-occlusion; also confirm the documented v1
  behavior that visible marker geometry is not magically filtered.
- A feed aimed at another Prism display does not recurse.
- Turn `RenderMirrors` off and reflection probes off; feeds remain functional.
- Toggle Prism master off/on and verify target release atomically reports
  `EMPTY/PAUSED`, bumps runtime revision, then waits for republish.
- Enable native mirrors/probes; feed state does not change their selection or
  output.
- Snapshot before/after aux: camera origin/axes/FOV/near/far/user plane;
  current/last/delta matrices; camera ID; FBO and `mRT`; viewport/scissor;
  shader; blend/depth/cull/front-face/color mask; underwater state; render mask;
  light identities/flags/fades; spot-shadow targets/fades; Poisson offset; clip
  and mirror uniforms.
- Force every early return after state entry, including scratch/output allocation
  failure.
- Instrument every direct and nested Prism target bind. After each bind verify
  viewport, scissor, `gGLViewport`, frustum aspect, and screen-size uniforms still
  equal the logical extent; also verify the loaded shader is
  `class1/deferred/prismLensF.glsl` and links with `prism_uv`.
- Put lights only near the main eye and only near the source eye; verify correct
  feed lighting, full auxiliary fade, correct handling of pre-existing nearby
  flags, and unchanged main light state.
- Put projectors near both eyes and prove every shadow-target and
  `mSpotLightPriority` value is unchanged by the auxiliary pass.
- Put source above/below water independently of the main camera.
- Exercise every opaque-water clip mode and prove feeds use the normal water
  plane but never the destination-face plane.
- Test source views outside main shadow cascades and document the result.
- Test glass, particles, alpha hair, and transparent objects both in the feed
  and in front of the display.
- Test objects behind the main camera and outside current interest/LOD range.

### Performance capture

Record Prism off, one producer with 1/8/16 bindings, and three producers at every
adaptive resolution/cadence rung:

- CPU cull/sort and draw-submission time;
- GPU G-buffer, lighting, copy, and composite time;
- p50 and p95 main render-work time plus presented-frame interval/FPS;
- per-capture refresh cadence and maximum gap;
- target reallocations;
- retained-target and scratch VRAM;
- avatar-heavy and alpha-heavy worst cases;
- mixed tiny/large and wide/tall displays that expose shared-scratch
  oversampling;
- CPU cost of the auxiliary light-candidate scan.

Controller-specific tests:

- inject main rates 30, 37, 45, 59.94, 60, 90, and 120 FPS for 60 seconds with
  Lens and Camera Feed configured Automatic or Target 30. One safe producer
  approaches its admitted value up to 30 and three equal requests approach their
  computed 10/10/10 entitlements;
- for every positive `e_i`, verify attempted long-run rate approaches `e_i` and,
  without failures/backoff, bound the ordinary attempt gap by
  `1/e_i + (|W|-1) * max(1/Rallowed, one_main_frame) + one_main_frame`. Test
  heterogeneous 5/25 and 5/10/15 entitlements rather than using an obsolete
  active-count/equal-share formula;
- insert a 500 ms stall and prove no catch-up burst;
- step no-capture load from 16 to 36 ms: suspend within 0.5 seconds, label
  baseline failure using the immediate diagnostic burst, and recover only after
  dwell; after every timing reset prove admission waits for a valid baseline;
- feed noisy 29–34 ms samples and prove no tier oscillation;
- combine one cheap, one medium, and one expensive capture in every slot order;
  global admission/rate is identical, individually unsafe captures do not block
  safe siblings, and per-row entitlement differs from observed publication;
- invalidate/expire a capture's cost model while global steady rate is zero and
  prove oldest-first minimum-tier discovery probes remain at least 2.1 seconds
  apart globally and all three pending producers receive a turn within 6.3
  seconds whenever the confident baseline itself fits `D`;
- use a 25 ms baseline, measured 10 ms high tier, and actual 3 ms minimum tier;
  prove the isolated probe discovers/adopts the safe tier rather than leaving the
  producer permanently throttled by the conservative downscale prediction;
- exercise GPU-bound and CPU/draw-bound captures. Equal clamped/bucketed extents
  never count as evidence; only two material pixel-area reductions with low
  normalized GPU response may skip later resolution rungs;
- withhold every GPU timestamp result and prove no blocking query read or GL
  stall; verify startup no-aux baseline, isolated probe plus two-frame wall-cost
  attribution, minimum quality, at most 5 Hz when CPU/wall admission passes, and
  zero when it fails;
- verify a logical 1024-to-256 downshift reduces viewport/scissor, resolution
  uniforms, and measured shaded pixels despite retained 1024 capacity;
- on target hardware require main render-work p95 `<= 33.33 ms` after stabilization whenever
  Prism-off baseline plus the minimum capture tier make it achievable; otherwise
  require explicit automatic suspension rather than a false guarantee.
- repeat deterministic deadline/emergency/recovery tests for 45 and 60 FPS,
  intentional VSync/frame caps below 30 (for a 20-FPS cap, every threshold uses
  `F_effective = 20`, `D = 50 ms`), runtime target changes, Adaptive↔Manual
  transitions, Manual `Every Frame`, and re-entry to Adaptive with the saved zero
  sentinel clamped effectively to 30 Hz;
- inject malformed saved settings: target zero/extreme integer, refresh outside
  the allowed set, non-finite quality, and out-of-range quality. Verify the
  normative fallbacks/clamp happen before division and emit only one warning.

Do not describe the feature as a “small performance hit” until this measurement
exists on target hardware.

## 23. Recommended implementation order

This should be delivered as one usable feature, but reviewed in risk-focused
internal checkpoints:

1. Split the registry/API into three capture producers and sixteen generation-
   safe display bindings; keep GL output ownership capture-only.
2. Implement binding-first preparation, maximum-demand aggregation, stable
   capture aspect, and source pose resolution once per producer.
3. Implement symmetric Camera Feed projection and Lens's single-aperture path.
4. Extend composite state/shader for capture-slot indexing, grouped fan-out,
   retained Lens orientation, Fit/Fill/Stretch, anchors, bars, and logical
   texture region.
5. Give each capture an exact-size cached scratch/water pack; keep viewport,
   scissor, screen-size uniforms, and normalized texture extents identical after
   every direct/nested target bind.
6. Add feed destination-plane exclusion, normal water clipping, source-eye
   behavior, marker/eye-offset contract, safe auxiliary lights, SSR/hero/noise,
   and every projector mutation guard.
7. Add nonblocking timestamp measurement, wall-clock scheduler, adaptive
   admission/scale/cadence, hysteresis, failure backoff, and truthful statuses.
8. Add complete Prism manager UI, atomic version-3 persistence/version rejection, logout
   cleanup, settings, and user documentation.
9. Perform adversarial review of the complete diff, including a dedicated
   weakest-target 30-FPS/fan-out performance pass.
10. Fix every must-fix finding and re-review until zero remain.
11. Only then build once. This brief itself does not authorize or perform a
    build.

## 24. Sources

Primary/official references:

- Blender Manual, Cameras — camera objects, perspective, focal length/FOV,
  sensor fit, and clip bounds: <https://docs.blender.org/manual/en/2.92/render/cameras.html>
- Blender Manual, UV Project Modifier — current Blender documentation explicitly
  identifies negative local Z as the emission/view direction for a camera or
  projector controller: <https://docs.blender.org/manual/en/dev/modeling/modifiers/modify/uv_project.html>
- Epic API, `USceneCaptureComponent2D` — a planar scene snapshot feeding a 2D
  render target: <https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Components/USceneCaptureComponent2D>
- Epic Scene Capture 2D example — explicitly compares the result to a camera
  feeding a TV/security monitor and contrasts it with cube capture:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/1.7---scene-capture-2d>
- Unity `Camera.targetTexture` — camera rendering directly into a render texture:
  <https://docs.unity3d.com/ScriptReference/Camera-targetTexture.html>
- Second Life Mirrors — confirms the native mirror feature is a different,
  reflection-probe-backed mechanism:
  <https://wiki.secondlife.com/wiki/Mirrors>

Local source inspection is the authority for Alchemy-specific axes, state,
selection, render targets, and integration contracts. Web references establish
the cross-engine camera-to-render-target pattern; they do not override the
checkout's actual APIs.
