# Prism Camera Feed integration handoff for Claude

Date: 2026-08-04

Repository: `I:\alchemy-machinima` (`develop`)

Status: source implementation candidate in the working tree. It has received
static and adversarial source review, but it has deliberately not been
configured, compiled, shader-linked, launched, or benchmarked.

Final adversarial static verdict: zero P0 defects, zero P1 defects, and zero
unresolved P2 source defects. This verdict is not a substitute for the compile,
GLSL-link, runtime-containment, UI, or performance gates below.

This is the authoritative handoff for the current implementation. It supersedes
`PRISM_LENS_THREE_INSTANCE_CLAUDE_HANDOFF.md`, which describes the older
Lens-only/shared-scratch architecture and has obsolete memory figures.

## 1. What was implemented

Prism now has two capture-producer modes:

- `SURFACE_LENS`: the existing surface-locked magnifier. Its aperture face is
  also its one required display.
- `CAMERA_FEED`: a Blender-style camera marker. The selected ordinary world
  object supplies the live camera transform, while independently selected prim
  faces display its retained image.

The bounded ownership model is:

- at most three capture producers total across both modes;
- at most sixteen display bindings total;
- one retained HDR output per occupied capture;
- one Camera Feed capture can fan out to many display faces;
- display bindings own mapping state only, never an auxiliary scene render or
  render target;
- at most one auxiliary scene-capture attempt is admitted in a main frame.

Consequently, one camera on ten faces performs one scheduled scene render and
ten comparatively cheap face composites. It does not perform ten scene
renders.

This is a real source patch, not pseudocode or a theory-only design. The
remaining uncertainty is compile/runtime validation because this task expressly
forbids building the client.

## 2. Camera and display semantics

A Camera Feed source:

- is resolved by UUID for every scheduled capture; no viewer-object pointer is
  retained across frames;
- reads `getRenderPosition()` and `getRenderRotation()`, so linked,
  attachment, and interpolated render transforms are honored;
- looks along local `-Z`;
- uses local `+Y` as image up;
- supports a local eye offset;
- uses a symmetric perspective projection;
- has configurable near/far clip and output aspect;
- supports fixed vertical FOV or Follow Projector FOV.

Follow Projector is valid only when the source has projector light parameters.
The Camera Feed is otherwise independent of lights, mirror flags, reflection
probe ownership, and simulator probe messages.

Each Camera Feed display independently stores:

- Fit, Fill, or Stretch;
- a two-axis crop/placement anchor;
- linear-RGB letterbox color.

The capture framing and retained output are shared. Adding, changing, or
removing a display does not create another camera render. A display face may
belong to only one capture.

A Surface Lens deliberately keeps its original one-aperture invariant. Its
required display cannot be removed independently, and Camera Feed mapping
controls do not apply to it.

## 3. Picture-rate and performance contract

Every capture has one output-rate policy inherited by all of its faces:

- `Automatic`;
- `Target FPS`, validated to 1 through 30 FPS.

The scheduler uses monotonic time, per-producer deadlines, retry backoff,
deterministic round-robin/overdue selection, and max-min water filling. A low
requested rate is satisfied before unused aggregate budget is shared among
higher-rate producers. Missed time does not create catch-up bursts.

Global controls are:

- Adaptive performance, on by default;
- protected main-view target: 30, 45, or 60 FPS;
- total auxiliary-attempt budget: 5, 10, 15, 20, or 30 attempts/s;
- a Manual-only Every Frame sentinel;
- quality ceiling, using the existing `PrismLensResolutionScale`.

Important truth boundary: the implemented adaptive controller is a bounded,
presented-FPS-only state machine. It samples `gFPSClamped` once per main frame.
Below `target - max(0.5 FPS, 2%)`, it filters ordinary dips for 0.2 seconds and
then clamps aggregate cadence and capture resolution downward; at or below 80
percent of target it can suspend new captures immediately while retaining the
last valid image. Recovery requires two sustained seconds above
`target - max(0.1 FPS, 0.5%)`, then increases cadence by at most 0.05 of full
capacity per second and resolution by at most 0.025 of full scale per second,
with no stall catch-up. A slow EWMA built only after at least eight contiguous
no-aux frames and 0.25 seconds may conservatively veto recovery when that
reference is itself below the recovery band; it never permits more work and is
not cost attribution. Mode, target, master, capture-empty, scene, and render-
resource transitions reset controller history.

The manager reports this as FPS-only protection and explicitly reports GPU
timing as unavailable. The patch still does not implement the research
document's proposed GPU timestamp ring or p95 render-work admission, and it
cannot identify Prism as the cause of a presented-FPS change.

Therefore 30 FPS is a protection target and best-effort operating mode, not a
guarantee. If the scene without Prism already misses 30 FPS, Prism cannot make
it reach 30. Final tuning requires a permitted build and benchmark on the
weakest supported GPU.

The expensive operation is still an additional cull/sort/G-buffer/deferred
lighting pass. It is not volumetric-projector-cheap. Lower picture FPS saves
scene captures; adding faces mostly adds compositor cost.

## 4. Resource bounds

The implementation uses:

- three fixed 1024-by-1024-capacity `GL_RGBA16F` retained outputs;
- three lazily allocated, exact-logical-size deferred scratch packs;
- matching per-capture exact-size water distortion/exclusion targets;
- logical capture axes bucketed from 64 through 1024;
- no render targets owned by the sixteen display records.

The exact-size packs avoid cross-capture resize churn and normalized-UV stale
texels. A zero-consumer camera keeps its definition and slot but releases its
capture-owned GPU scratch/output; a later visible binding regenerates it.

Worst-case target storage with all three captures at 1024 square is
approximately 183-207 MiB before driver/FBO overhead. Typical use is lower
because scratch/water packs are lazy and exact-size. Do not use the old
60-66 MiB estimate.

## 5. Remote-view containment

The auxiliary pass is enclosed by `ScopedPrismRenderState` plus pipeline-owned
snapshot/restore helpers. The patch preserves or restores:

- viewer camera and camera ID;
- current, last, and GL projection/model-view matrices;
- delta matrices;
- render-target pack and currently bound target;
- viewport, scissor, clear color, color mask, blend factors, and matrix mode;
- render-type mask, occlusion mode, underwater state, visible counters;
- nearby-light set and CPU-side hardware-light records;
- sun/moon light values and directions;
- projector-shadow targets, priorities, fades, and Poisson offset;
- reflection-probe UBO contents;
- the exact main-view sky/water uniform-cache backing stores;
- the camera-dependent water plane and classic-sky mode;
- current shader and the explicit Prism auxiliary shader flag.

The source-eye view receives an independently built nearby-light set without
changing persistent drawable membership/fade state. Candidate lights are
filtered against source-camera range/frustum before the bounded light budget is
filled, preventing off-camera lights from starving visible ones.

Projector priority/shadow-selection mutations are disabled during the auxiliary
pass. Shared Poisson noise does not advance. The class-3 reflection shader
receives `prism_auxiliary`; both SSR paths and all hero-probe taps are skipped
for the remote view, and the value is explicitly restored for a directly rebound
main shader.

The ordinary probe system can still provide unrelated PBR IBL. Camera Feed does
not create or own probes and remains a distinct feature from SL's
probe-dependent mirrors.

Water height, underwater classification, alpha grouping, water-plane clipping,
water rendering, and water/fog settings use the source camera's region during
the auxiliary pass and return to the main eye afterward.

Environment uniforms use a bounded transaction rather than rebuilding the
main-view maps on restore. `LLEnvironment::swapShaderUniformState()` swaps the
main sky/water vector backing stores into persistent scratch, builds the
source-eye maps once, and swaps the exact main maps back before rebinding the
saved shader. The transaction snapshots every unique base, rigged, and GLTF
shader dirty bit. A forced bit cleared during the synchronous auxiliary pass
proves that shader consumed source-eye maps; restore therefore uses
`saved_dirty || auxiliary_maps_applied`. Unused shaders recover their exact
prior bit, while only shaders that consumed auxiliary maps receive one deferred
main-view reapply. The record vector retains capacity between captures and
does not retain shader pointers after a scope ends. Deferred screen-uniform
tracking is cleared specifically when shader programs unload; render-target-
only resets retain it because live programs may still need one main-view
dimension restore.

## 6. Registry, lifetime, and persistence

`LLPrismLens` exposes fixed-size snapshot/value APIs. Persistent editor handles
contain a UUID plus a monotonically allocated process-lifetime generation, so a
stale floater selection cannot mutate a replacement record.

Offline source/display UUIDs are retained instead of being silently deleted.
Health, output, activity, display health, and display visibility are reported as
separate states. Master-off preserves definitions but releases GPU resources.
Logout clears the session registry/runtime state.

Director scene schema is version 3:

- `prism_captures` stores persistent capture IDs, mode, common rate intent, and
  Camera Feed optics/source fields;
- `prism_displays` stores persistent binding IDs, capture references, object
  UUID/face, and mapping settings;
- runtime slots, generations, deadlines, outputs, and telemetry are not saved;
- both arrays are parsed into temporary records and fully validated before one
  registry replacement;
- malformed, truncated, unsupported-version, duplicate, non-finite,
  out-of-range, or referentially invalid data leaves the live Prism state
  untouched.

Scene save serializes to a same-directory temporary file, flushes/closes it, and
then calls `LLFile::rename(temp, final)`. That utility replaces an existing
destination in this codebase. POSIX provides atomic visibility; the underlying
Windows APIs do not offer the same crash-atomicity guarantee, but this
no-predelete path is the safest existing project utility and preserves the old
file on serialization/write failure.

Version 1 has no Prism payload and preserves the live registry. Version 2 was
never a shipped Prism schema; version 2 and unknown future versions preserve
the live Prism registry, warn, and continue through the existing tolerant load
path for non-Prism legacy scene fields.

## 7. Floater and scrolling work

`LLFloaterPrismManager` is a real registered, CMake-listed, resizable floater,
opened from Director's compact Prism summary. It provides:

- capture and display registries;
- Add Camera, Add Lens, rebind camera, add/remove display, and confirmed capture
  cascade removal;
- Camera Feed optics, aspect, Fit/Fill/Stretch, anchor, and bar controls;
- common Automatic/Target FPS controls for both modes;
- adaptive/manual global controls;
- live requested/entitled/observed rate, output age, health, activity, and
  performance readouts.

The capture and performance forms use page-local scroll containers with
definite-height document panels. Capture/display lists keep their own
scrollbars. Focus callbacks convert descendant rectangles into document
coordinates and call `scrollToShowRect()`, keeping keyboard-focused controls
visible.

Preserve the manager tab design rectangles (`878x373`). A direct tab child with
no explicit size is initially constructed at the UI fallback height of 10 px;
that corrupts the saved offsets of descendants that follow the bottom before
the tab container performs its final reshape. The Displays tab additionally
uses a vertical layout stack: only `display_list_layout` auto-resizes, while the
24 px face-action row and 96 px mapping editor remain fixed and reachable. At
the declared minimum floater height, the list still receives 51 px against its
48 px minimum. This is what keeps **Add selected face** visible at restored,
default, minimum, and enlarged floater rectangles.

The standalone floater audit also repairs scrolling/focus behavior in the Actor
Mover, Cinematic Camera, Director, Ghost Studio, Lightbox, Phototools, Prop
Mover, and Temporal Capture surfaces. The shared helper is
`indra/newview/alscrollfocus.h`; the audit and test matrix are in
`doc/MACHINIMA_STANDALONE_FLOATER_SCROLL_AUDIT.md`.

## 8. Files Claude should integrate

Review and stage explicit paths/hunks. The worktree contains unrelated user
files; never use `git add -A`.

Core registry, scheduler, camera path, and compositor:

- `indra/newview/llprismlens.h`
- `indra/newview/llprismlens.cpp`
- `indra/newview/pipeline.h`
- `indra/newview/pipeline.cpp`
- `indra/llrender/llrender.h`
- `indra/llrender/llrender.cpp`
- `indra/newview/llenvironment.h`
- `indra/newview/llenvironment.cpp`
- `indra/newview/llviewershadermgr.cpp`
- `indra/newview/lldrawpoolalpha.cpp`
- `indra/newview/lldrawpoolwater.cpp`
- `indra/newview/llsettingsvo.cpp`

Shaders:

- `indra/newview/app_settings/shaders/class1/deferred/prismLensF.glsl`
- `indra/newview/app_settings/shaders/class3/deferred/reflectionProbeF.glsl`

Manager, Director, persistence, settings, and lifecycle:

- `indra/newview/llfloaterprismmanager.h`
- `indra/newview/llfloaterprismmanager.cpp`
- `indra/newview/skins/default/xui/en/floater_prism_manager.xml`
- `indra/newview/llfloaterdirector.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/llviewerfloaterreg.cpp`
- `indra/newview/llappviewer.cpp`
- `indra/newview/app_settings/settings.xml`
- `indra/newview/CMakeLists.txt`

Standalone-floater scrolling:

- `indra/newview/alscrollfocus.h`
- the changed Actor Mover, Cinematic Camera, Ghost Studio, Lightbox,
  Phototools, Prop Mover, and Temporal Capture C++/XUI files shown by
  `git status --short`;
- `doc/MACHINIMA_STANDALONE_FLOATER_SCROLL_AUDIT.md`.

Research and integration documentation:

- `doc/PRISM_VIRTUAL_CAMERA_SURFACE_DEEP_RESEARCH.md`
- `doc/PRISM_LENS_SURFACE_FIT_DEEP_RESEARCH.md`
- this handoff.

The existing `llviewerdisplay.cpp` Prism hook calls
`LLPrismLens::renderAuxiliaryView()`; it is not changed by this revision.

Do not stage unrelated `CLAUDE.md`, crash dumps, `.tmp.driveupload`, or the
unrelated Froxel/probe-mirror notes unless their owner separately requests it.

## 9. Recommended integration order

1. Integrate `LLRender` light snapshot support and the pipeline containment,
   per-capture render-target, water, and compositor changes.
2. Integrate `LLPrismLens` capture/display registry, camera preparation,
   scheduler, persistence data model, and auxiliary pass.
3. Integrate the two shader changes together with their uniform-upload sites.
4. Integrate settings, Director v3 persistence, logout cleanup, manager class,
   floater registration, XUI, and CMake membership.
5. Integrate standalone-floater scrolling as a separate reviewable group.
6. Re-run static XML/source checks.
7. Only with authorization, configure and compile once; fix compiler and shader
   errors before changing behavior.
8. Run the acceptance matrix below before calling the feature production-ready.

## 10. Required post-build validation

No item in this section was executed by Codex because building/running was out of
scope.

- Verify local `-Z` forward and `+Y` up with asymmetric text.
- Move/rotate camera and display independently; test fixed and projector FOV.
- Fan one camera to 1, 8, then 16 faces and instrument that one publication
  equals one auxiliary scene render in every case.
- Test mixed Fit/Fill/Stretch, anchors, bars, back faces, and output aspects.
- Verify fourth capture, seventeenth display, duplicate face, invalid source,
  stale handle, and Lens display removal are rejected without mutation.
- Test Automatic plus 5/10/15/20/23.976/24/25/29.97/30 FPS targets; compare
  requested, entitled, and observed values and verify no catch-up burst.
- Snapshot main camera/GL/light/probe/projector/water state before and after every
  successful and failed auxiliary path.
- Aim remote cameras at source-only lights and across region water heights.
- Disable probes and native mirrors; Camera Feed must remain functional.
- Test the manager and all audited floaters at minimum size and 100/125/150/200
  percent UI scale with wheel, scrollbar, keyboard Tab, and error reveal.
- Round-trip v3 scenes; feed truncated XML, malformed numerics, bad references,
  version 2, and a future version, proving live state is unchanged on failure.
- Benchmark Prism off, one camera with 1/8/16 faces, and three captures at every
  rate/quality tier. Record CPU submission, GPU frame time, p50/p95 presented
  time, publication cadence, composite cost, allocations, and VRAM.
- On the weakest target, verify Adaptive reduces scale/cadence and ultimately
  suspends capture when needed. Do not advertise a guaranteed 30 FPS without
  these measurements.

## 11. Known first-version limits

- main-view shadow maps are reused, so a remote angle can have incomplete
  shadows;
- LOD, avatar range, alpha ordering, and simulator interest remain main-view
  influenced;
- a visible camera-marker prim is not automatically hidden from its own feed;
- feeds do not recurse through other Prism screens;
- no orthographic camera, depth of field, motion-blur history, or lens
  distortion;
- transparent geometry in front of a late-composited display may sort
  imperfectly;
- user-set Target FPS and protected main FPS remain conditional on main-frame
  opportunities and hardware headroom;
- Windows scene-file replacement is functionally safe on ordinary success/error
  paths but is not claimed crash-atomic across power loss.

Do not weaken containment or turn any of these limits into a silent global-state
mutation to make a demo appear more complete.
