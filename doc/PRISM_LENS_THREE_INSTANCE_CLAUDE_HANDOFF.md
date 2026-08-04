# Prism Lens three-instance integration brief for Claude

> **Superseded (2026-08-04):** Do not integrate from this document. It describes
> the earlier Lens-only/shared-scratch design and its 60-66 MiB estimate is no
> longer true. Use `doc/PRISM_CAMERA_FEED_CLAUDE_HANDOFF.md` for the implemented
> three-capture Camera Feed + Surface Lens architecture, exact-size per-capture
> scratch packs, sixteen display bindings, and current validation status.

**Date:** 2026-08-03

**Repository:** `I:\alchemy-machinima` (`develop`)

**State:** implementation and adversarial static review complete; intentionally
not built or run by Codex.

## Executive handoff

The working tree now contains the Model-A, surface-locked Prism Lens renderer
with a fixed practical limit of three designated faces. It is no longer a
single global lens.

The performance policy is deliberately bounded:

- prepare all currently resolvable registered faces;
- render at most **one** complete auxiliary camera view per viewer frame;
- retain one independent linear-HDR beauty per lens slot;
- composite every visible lens that has a valid retained beauty;
- refresh the most overdue visible slot, with failure backoff;
- cap every target axis at 1024;
- release all Prism render targets on the enabled-to-disabled transition while
  preserving the three local registry identities.

This is not volumetric-projector-cheap. While a lens is actively updating, the
dominant cost is one additional cull/sort/opaque/deferred-lighting scene render
per frame. Lenses two and three share that render budget; they do not add a
second and third full scene render. Worst-case Prism target memory is roughly
60–66 MiB before driver/FBO overhead.

## User-visible behavior

The Director Camera tab now provides:

- `Prism lenses N/3` master checkbox;
- global `Zoom all` and `Quality all` controls;
- `Add selected face`;
- `Remove highlighted`;
- `Clear all`;
- a fixed three-row registry list showing slot, UUID prefix, and face index.

Registry ownership is independent of `PrismLensEnabled`. Turning rendering off
does not erase the list. Registry entries are session-local and keyed by
`(object UUID, texture-entry index)`.

Adding requires exactly one selected world object and one selected face. The
face must be a non-rigged, rectangular, approximately planar affine UV surface.
Camera-independent surface validation happens before a slot is consumed or the
UI reports success. Duplicate adds are idempotent; a fourth add is rejected
without evicting anything.

A UUID temporarily absent from `gObjectList` is treated as streaming/range/
region absence and retained. A known dead, HUD, non-volume, rigged, malformed,
or no-longer-compatible face invalidates only its own slot. Offscreen, tiny,
near-clipped, or rebuilding faces are transient skips and retain their cache.

## Load-bearing renderer design

`LLPrismLens::MAX_LENSES` is a compile-time constant of three. The registry uses
three fixed slots and the pipeline owns:

- one shared deferred scratch `RenderTargetPack` (`mPrismLensRT`);
- three fixed `GL_RGBA16F` retained outputs (`mPrismLensOutput[3]`).

Do not replace the fixed output array with a movable/erasable vector.
`LLRenderTarget` owns raw GL resources and is not safe to move as an ordinary
value.

Each frame:

1. `llviewerdisplay.cpp` calls `LLPrismLens::renderAuxiliaryView()` before the
   main `stateSort()`.
2. The manager resolves and surface-prepares each occupied slot against the
   current main camera.
3. It selects one missing/oldest visible output.
4. `ScopedPrismRenderState` and the active-slot guard install the temporary
   camera, matrices, viewport, target pack, user clip plane, and shader-uniform
   state.
5. A generalized off-axis projection renders through the central `1/zoom`
   sub-quad of that face.
6. Deferred lighting finishes and flushes the scratch HDR beauty.
7. Scratch is copied/downsampled into that slot's independent logical output
   while the complete state scope is still active.
8. The scope restores the main renderer; only then is the output generation
   marked valid.
9. During main deferred lighting, each current real face composites from its
   matching retained output before the main HDR screen flush and tonemap.

The shared scratch grows immediately when the scheduled slot needs a larger
bucket. It shrinks only after 120 consecutive scheduled updates with a smaller
request. Each retained output grows independently, so a 1024 lens does not force
the other two outputs to 1024. Scratch/output allocation failures back off and
reset the failed scratch request so a large failing slot cannot starve smaller
ones.

Capture-time UV scale/offset is stored with each output. This is essential: if
the eye crosses a lens plane while that slot is waiting for its next update,
sampling must use the orientation of the cached capture rather than the newly
prepared frame or the retained image mirrors for one or two frames.

## Invariants that must not be weakened

- Temporary lens FOV changes use `setViewNoBroadcast()` only. Never call
  `LLViewerCamera::setView()` from the auxiliary path; it sends `AgentFOV`.
- Every camera, matrix, viewport, scissor, target, blend/mask, shader, clip-
  uniform cache, visible-counter, and related mutation remains inside the full
  scoped restore.
- Output copy occurs after deferred lighting flushes scratch and inside that
  scope. Publication occurs only after scope destruction.
- The active registry slot is installed before Prism clip uniforms are dirtied.
- `pipeline::updateCull()` must preserve the Prism user clip plane while
  `sPrismLensRender` is true.
- The fragment clip and CPU user plane keep geometry on the far side of the
  physical lens and reject camera-side foreground from the auxiliary view.
- The composite remains linear HDR, on the real face, before main screen flush,
  bloom, and tonemap.
- One complete matrix/GL restore object is created per composite face.
- Prism never recursively invokes another Prism update/composite.
- Registry edits never write `PrismLensEnabled`.
- Do not modify Ghost Studio clone/overlay/actor-ghost paths for this feature.

## Files to integrate

The worktree is dirty for unrelated reasons. Do **not** use `git add -A` or
wholesale-stage mixed files. Review and stage Prism hunks explicitly.

Core three-instance implementation:

- `indra/newview/llprismlens.h`
- `indra/newview/llprismlens.cpp`
- `indra/newview/pipeline.h`
- `indra/newview/pipeline.cpp`

Shader and behind-lens clip integration:

- `indra/llrender/llshadermgr.h`
- `indra/llrender/llshadermgr.cpp`
- `indra/newview/llviewershadermgr.h`
- `indra/newview/llviewershadermgr.cpp`
- `indra/newview/llsettingsvo.cpp`
- `indra/newview/app_settings/shaders/class1/deferred/prismLensV.glsl`
- `indra/newview/app_settings/shaders/class1/deferred/prismLensF.glsl`

Settings and Director UI:

- `indra/newview/app_settings/settings.xml`
- `indra/newview/llfloaterdirector.h`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/skins/default/xui/en/floater_director.xml`

Documentation:

- `doc/PRISM_LENS_SURFACE_FIT_DEEP_RESEARCH.md`
- `doc/PRISM_LENS_DEEP_RESEARCH.md`
- `doc/MACHINIMA_USER_GUIDE.md`
- this handoff file

The two shader files and the surface-fit research document are currently
untracked, as is this handoff, so they require explicit staging if this becomes
a commit.

Existing prerequisites already present in the branch (verify them if porting
to another branch; they currently have no separate working-tree diff):

- `indra/newview/CMakeLists.txt` lists `llprismlens.cpp/.h`;
- its recursive shader glob includes new `.glsl` sources in the IDE project but
  does not itself guarantee they reach the ordinary local runtime tree;
- `indra/newview/llviewercamera.h` defines `CAMERA_PRISM_LENS`;
- `indra/newview/llviewerdisplay.cpp` includes `llprismlens.h`, calls
  `renderAuxiliaryView()` before main state sort, and calls `compositeDebug()`
  after deferred lighting;
- deferred `globalF.glsl` retains the mirror/clip-plane predicate reused by the
  Prism auxiliary pass.
- `indra/newview/viewer_manifest.py` copies the shader directory only for
  package actions; the ordinary build copy path may omit newly added shaders.

The reserved-uniform enum insertion in `llshadermgr.h` and the
`"prismLensMap"` insertion in `llshadermgr.cpp` must remain at the same ordinal.
If only one side is integrated, every reserved uniform after it becomes
misindexed.

Do not include unrelated untracked `.tmp.driveupload`, crash dumps,
`.codex_prism_*`/`.codex_clone_*` notes, or `doc/FROXEL_AND_PATH_SCOPE.md` in the
Prism integration.

## Important current anchors

Reconfirm line numbers after merge conflict resolution:

- hard cap/API: `llprismlens.h:20-73`;
- surface validation: `llprismlens.cpp:495`;
- registry and designation semantics: `llprismlens.cpp:693`;
- scheduler/cache bookkeeping: `llprismlens.cpp:1120`;
- full render-state scope: `llprismlens.cpp:1450`;
- one scheduled auxiliary update: `llprismlens.cpp:1694`;
- multi-face composite state generation: `llprismlens.cpp:1958`;
- scratch/output allocation: `pipeline.cpp:1338-1434`;
- Prism-preserving cull path: `pipeline.cpp:3533`;
- HDR multi-composite: `pipeline.cpp:16267`;
- active fragment clip uniform: `llsettingsvo.cpp:1135`;
- shader creation: `llviewershadermgr.cpp:1369`;
- Director handlers/list: `llfloaterdirector.cpp:1927-2017`;
- Director XUI: `floater_director.xml:1063-1152`.

## Integration sequence for Claude

1. Preserve the dirty tree and inspect Prism hunks file by file. Use
   `git add -p` for mixed files. Do not discard or reformat unrelated edits.
2. Confirm the three no-diff prerequisites above are still present after any
   rebase/cherry-pick.
3. Confirm both new shader files exist in the source shader tree before running
   the viewer. The composite shader is loaded unconditionally with deferred
   shaders; a missing/broken runtime shader can fail viewer shader startup even
   though the C++ build succeeds.
4. Run non-build checks first:

   ```powershell
   git diff --check
   $null = [xml](Get-Content -Raw -LiteralPath `
     'I:\alchemy-machinima\indra\newview\app_settings\settings.xml')
   $null = [xml](Get-Content -Raw -LiteralPath `
     'I:\alchemy-machinima\indra\newview\skins\default\xui\en\floater_director.xml')
   ```

5. With the viewer closed, build when authorized:

   ```powershell
   cmake --build "I:\alchemy-machinima\build-Windows-vs2026-os" `
     --config Release --target alchemy-bin
   ```

   A trailing vcpkg `z-applocal`/MochaPro-manifest `MSB3073` can be benign only
   if linking actually succeeded. Verify the timestamp of
   `build-Windows-vs2026-os\newview\Release\AlchemyTest.exe`.

6. Verify runtime assets, especially the new GLSL files. If the manifest did
   not stage them, copy them explicitly into:

   `build-Windows-vs2026-os\newview\Release\app_settings\shaders\class1\deferred\`

   Also verify the updated Director XUI and settings XML reached the Release
   runtime tree.
7. Launch, inspect `Alchemy.log` for `Prism Lens Composite Shader`,
   `prismLensV.glsl`, `prismLensF.glsl`, shader compile/link errors, GL errors,
   and `PrismLens` warnings before visual testing.
8. Run the acceptance matrix below before staging a final integration commit.

## Runtime acceptance matrix

### Registry and UI

- Start with `PrismLensEnabled=false`: Director opens, list is empty, no shader/
  XUI error occurs.
- Select exactly one simple rectangular planar prim face and add it.
- Add the same face again: no duplicate and no mutation.
- Select multiple objects/faces: Add is disabled with a specific reason.
- Add three different valid faces: list shows three stable slots.
- Attempt a fourth: it is rejected and the original three remain.
- Remove the highlighted middle slot, then add another face: only the free slot
  is reused.
- Clear all: registry and Prism targets clear without changing unrelated
  settings.
- Add while master rendering is off: registry changes, rendering does not turn
  itself on.

### Surface-fit and rendering

- Test head-on, yawed, pitched, rolled, and back-side viewing. The image fills
  the face and remains locked to its geometric UV surface.
- Orbit across the face plane between cached updates. No one-frame mirror/jump
  should appear from using the wrong cached orientation.
- Test Zoom 1, 2, and 8. The source sub-frustum changes while the full face stays
  filled.
- Test Quality 0.25, 1.0, and 2.0. Targets remain bucketed and capped at 1024.
- Put an opaque object between camera and lens: it occludes normally in the main
  view and must not reappear magnified elsewhere inside the lens.
- Put geometry behind the lens: it appears in the magnified auxiliary view.
- Confirm three faces show three distinct cached views, not three copies of the
  most recently rendered scratch buffer.
- Set `PrismLensDebug` through the debug setting/control console (there is no
  Director checkbox for it) and confirm it shows the most recently refreshed
  retained output.

### Scheduling, state, and resource lifecycle

- With one visible lens, it may refresh every frame.
- With three visible lenses, observe the most-overdue schedule cycling without
  starvation; only one auxiliary cull/sort/deferred render should occur in any
  frame.
- Move a large lens through 256/512/1024 bucket boundaries. Scratch allocation
  must not oscillate every frame.
- Combine one 1024-request lens with small lenses. Small retained outputs must
  remain independently sized.
- Hide, distance-cull, or temporarily rebuild one lens. Other lenses continue;
  the transient slot remains registered.
- Leave a registered UUID temporarily unavailable, then return it to range. The
  row remains removable and resumes when the object resolves.
- Toggle the master off after all three outputs exist. Prism VRAM should be
  released once, the three rows should remain, and subsequent disabled frames
  should do no Prism preparation/render/composite work.
- Re-enable: retained outputs repopulate through the one-per-frame scheduler.
- Verify the main camera, viewport, scissor, color mask, shader, target stack,
  water state, main temporal matrices, and frame output remain stable after each
  auxiliary update.
- Verify no repeated `AgentFOV` traffic occurs while enabling, moving, zooming,
  or updating lenses.

### Failure cases

- Rigged, HUD, non-volume, non-planar, degenerate, malformed-UV, and
  non-rectangular faces are rejected before `Added` is reported.
- Force or simulate a large allocation failure if practical. The failing slot
  backs off; a smaller slot must still update rather than inheriting the failed
  1024 request forever.
- Missing/broken composite shader must be diagnosed as a runtime shader asset/
  compile problem, not mistaken for a successful C++ integration.

## Explicitly out of scope

- correct main-view ordering for blended transparent foreground;
- physical thin-lens refraction or bent rays;
- recursive Prism-in-Prism rendering;
- per-lens zoom/quality controls (current controls apply globally);
- implemented edge-feather behavior (`PrismLensEdgeFeather` is reserved);
- Ghost Studio scene-dressing integration inside the auxiliary view.

Use simple, low-tessellation rectangular faces for predictable preparation CPU.
Surface validation remains `O(vertices + triangles)` per registered, resolvable
face while Prism is enabled, although projected-footprint clipping itself is now
constant-size using the validated four-corner hull.

## Static review already completed

Codex ran two adversarial correction rounds and a final clean pass split across
render/state, performance/resource scheduling, and UI/API/settings concerns.
The final tree passed:

- `git diff --check`;
- settings XML parse;
- Director XUI parse;
- API/call-site and shader-uniform inventory checks;
- checks for the fixed three-target bound, one render call site, retry fairness,
  cached capture orientation, and master-off resource release;
- confirmation that the auxiliary path calls `setViewNoBroadcast()` and does
  not write the master setting.

No C++ build, runtime shader compile, viewer launch, or visual/GPU validation has
been performed. Those are Claude's remaining integration responsibilities.
