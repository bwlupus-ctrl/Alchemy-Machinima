# Scene-Lit Clone — P0 Implementation Blueprint (Codex, 2026-07-22)

Code-shape blueprint for P0, ready to implement. Follows `doc/SCENE_LIT_CLONE_P0P1_PLAN.md`. Source:
Codex implementation-design consult 2026-07-22. **Implement from this, then re-defer the diff for
review (mandatory loop). No code yet.**

## Confirmed decomposition — P0 = PROVEN-INERT HARNESS
P0's submission body issues **zero GL draw calls**. It validates: harvest timing, queue build/cull,
frame/view lifetime stamps, counters, invariant capture, and the G-buffer comparison machinery. P1
later replaces ONLY the no-op body with whitelisted opaque/masked draws; the same harness validates it.

Acceptance progression:
```
P0: WHOLE G-buffer OFF == ON, actualDrawCalls == 0
P1: outside clone bounds OFF == ON, clone region differs as expected
P1 two-clone: both separated bounds differ, outside their union stays identical
```
Qualification: OFF-vs-ON across two ordinary frames is NOT automatically deterministic. P0 must
classify camera/view/viewport/scene instability as **INCONCLUSIVE**, never PASS. PASS = inputs matched
AND every tested G-buffer value matched.

## Homes (do NOT create a standalone renderer singleton — it'd have to be dismantled for mirrors)
- **`llactormover.{h,cpp}`** — harvested `GhostBatch`/`GhostStaticFace` (exist); new frame-local
  `GhostProxy`, `GhostProxyQueue`, `GhostDeferredCounters`; `buildGhostDeferredQueue(const LLCamera&,
  U32 view_stamp)` + CPU frustum cull; `getGhostDeferredQueue(...)`; counter accessor.
- **`pipeline.{h,cpp}`** — the STABLE entry point `renderGhostDeferredOpaqueMasked(const LLCamera&)`
  (P0 no-op consumption; P1 fills draw) + `captureGhostDeferredContaminationSampleIfRequested(...)`.
- **NEW `llghostdeferreddiagnostics.{h,cpp}`** — `LLScopedGhostRenderInvariant` + the contamination
  test state machine + readback storage + verdict/reporting. Keeps diagnostic bulk out of pipeline.cpp.

Locked constraint honored: entry point takes **`const LLCamera&`** (keep const even though
`renderGeomDeferred` takes `LLCamera&`); no hard-coded world camera → mirror path reuses it.

## 1. Harvest move (llviewerdisplay.cpp ~915, the world stateSort block)
Inside the `stateSort`/`rebuildPools` block, AFTER `rebuildPools()`:
```cpp
gPipeline.stateSort(*LLViewerCamera::getInstance(), result);
if (rebuild) { gPipeline.rebuildPools(); }
// [GhostDeferred/P0] harvest after the final world pool rebuild; refs are frame-local.
LLActorMover::instance().collectGhostBatches();
```
Remove the existing late call (~1086, before `render_ui()`). Order becomes: world stateSort → optional
rebuildPools → collectGhostBatches → deferred world render → ghost queue consumption → render_ui →
clearReferences. Hazards: HUD stateSort — none (collector walks source attachment `mDrawMap`, not
`LLCullResult`); `clearReferences()` — safe because consumed same iteration (assert frame stamp every
access). **The 2nd deferred pass (~1276, teleport/transition) is EXCLUDED in P0** (documented); its own
view stamp must fail if someone later submits there without building.

## 2. Frame-local records (llactormover.h) — queue stores POINTERS, never copies
```cpp
enum class EGhostProxyPose : U8 { LIVE, FROZEN };
struct GhostProxy {
    LLUUID mInstanceId, mSourceId, mWearerId;
    LLVOAvatar* mSourceAvatar = nullptr;                 // non-owning, valid only for mFrameStamp
    const std::vector<GhostBatch>* mBatches = nullptr;
    const std::vector<GhostStaticFace>* mStaticFaces = nullptr;
    LLVector3 mFootAgent; LLQuaternion mRotation; F32 mScale = 1.f;
    LLMatrix4 mPlacement;                                // T(foot)*R*S*T(-source_foot_pivot)
    LLVector4a mWorldBoundsCenter, mWorldBoundsHalfExtent;
    EGhostProxyPose mPose = EGhostProxyPose::LIVE;
    bool mFrustumVisible = false;
    U32 mFrameStamp = 0, mViewStamp = 0;
};
struct GhostProxyQueue {
    std::vector<GhostProxy> mProxies; U32 mFrameStamp = 0, mViewStamp = 0;
    const LLCamera* mCameraIdentity = nullptr; void clear();
};
enum : U32 { GHOST_VIEW_WORLD_MAIN = 1, GHOST_VIEW_TRANSITION = 2, GHOST_VIEW_HERO_PROBE_BASE = 0x100 };
```
- Frame stamp = `LLFrameTimer::getFrameCount()` (NOT `gFrameCount` — codebase uses the former here).
- Validate at consumption: frame == current && viewStamp == expected && cameraIdentity == &camera; else
  `++mStaleQueueSkips`, `LL_WARNS`, `llassert(false)`.
- Bounds: `avatar->getLastAnimExtents()` [min,max] → transform 8 corners by placement → agent-space AABB;
  include static-face extents if larger, else `++mIncompleteBounds` + conservatively expand.
- Cull: `camera.AABBInFrustum(center, halfExtent) != 0`. **Add a `const` overload to `LLCamera`**
  (cleaner than const_cast) since AABBInFrustum is observational.
- The referenced vectors must NOT be cleared/reallocated between build and submission.

## 3. `LLScopedGhostRenderInvariant` (llghostdeferreddiagnostics.h) — diagnoses, does NOT restore
Scoped RAII capturing before/after: draw+read FBO, viewport, scissor enable+box, matrix mode,
modelview/projection/texture[0..3] (via `gGL.getModelviewMatrix()`/`getProjectionMatrix()`/
`getMatrixMode()` + **new `LLRender::getMatrix(eMatrixMode)`** for texture mats — NOT
`glGetFloatv(GL_TEXTURE_MATRIX)`, invalid in core), color mask, depth enable/write/func, blend
enable/func/equation, cull enable/mode/frontface, stencil front+back, program (`GL_CURRENT_PROGRAM` +
`LLGLSLShader::sCurBoundShaderPtr`), active texture (`GL_ACTIVE_TEXTURE`), pipeline flags
(`sUseOcclusion`, `sShadowRender`, `sImpostorRender`, `sReflectionRender`, `sRenderDeferred`, `sNoAlpha`,
`RenderSpotLight`), camera addr + `sCurCameraID` + view stamp. FBO via `LLRenderTarget::sCurFBO` verified
with `glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING)`. Full blend/stencil/cull/scissor/mask need raw
`glGetBooleanv`/`glGetIntegerv` (core, valid, but costly → gate behind the debug setting).
- On any changed field: emit ONE `LL_WARNS("GhostDeferredInvariant")` listing before→after, then
  `++*mViolationCounter; llassert(false);` (NOT `llassert_always` — production counts/drops, no abort).
- Call `finish()` explicitly before scope exit; destructor calls it only as a safety net.
- Do NOT restore pipeline globals — restoration would conceal P1 contamination.

## 4. `GhostDeferredCounters` (llactormover.h)
Fields: frameStamp; liveInstancesConsidered, frozenInstancesRejected, unresolvedSources; proxiesBuilt/
FrustumCulled/Visible/Submitted; riggedBatchesReferenced, staticFacesReferenced; acceptedOpaque/
acceptedMasked/rejectedBlend/rejectedGlow/rejectedUnknown batches; acceptedStaticFaces/rejectedStaticFaces;
nullPointerSkips, staleQueueSkips, incompleteBounds; **actualDrawCalls (MUST stay 0 in P0)**;
invariantViolations; contaminationPass/Fail/Inconclusive. `reset(frame)` once from
`buildGhostDeferredQueue()` when frame differs. Emit once after submission, gated on
`GhostDeferredDebugLog` (new Boolean setting, default 0). P0 warns if actualDrawCalls != 0.

## 5. G-buffer contamination harness (llghostdeferreddiagnostics.cpp)
`mRT->deferredScreen` attachments: A0 `GL_SRGB8_ALPHA8` base/albedo; A1 `GL_RGBA` spec/PBR-ORM;
A2 normal (`GL_RGBA16` / `GL_RGB10_A2` non-HDR); A3 optional emissive (exists only if
`RenderEnableEmissiveBuffer` at allocation — absence is not failure); shared depth. Readable before
`renderDeferredLighting()` while `deferredScreen` bound, via FBO + `glReadBuffer(GL_COLOR_ATTACHMENTn)`
+ `glReadPixels` (depth: `GL_DEPTH_COMPONENT/GL_FLOAT`). Synchronous readback OK behind a one-shot gate;
never every frame.
- Driver via new setting **`GhostDeferredContaminationTest`** (S32: IDLE=0/ARM=1/CANCEL=2). Internal
  phases: IDLE → WAIT_STABLE → CAPTURE_OFF → CAPTURE_ON → COMPARE. The test toggles ONLY the new
  submission gate (`ghost_submission_enabled_for_test`), never the user's Studio config. P0 body is
  no-op in both phases.
- Each capture records: viewport/resolution, camera addr+category, modelview+projection, queue identity
  + proxy transforms/bounds, attachment count/formats, stability cookie, raw attachment rects.
- Screen-space mask per visible proxy: 8 AABB corners → transform by the CAPTURED proj/modelview (not
  the singleton helper) → near-plane clip (don't just drop behind-camera corners) → perspective divide
  → NDC→captured viewport → integer rect → expand ≥2px (`GhostDeferredDiffMaskPad`) → clamp → union in a
  CPU byte mask. Near-plane crossing that can't produce a trustworthy rect → full-viewport mask or
  INCONCLUSIVE; never under-mask.
- Diff outside union: integer attachments exact byte equality; float emissive matched representation;
  depth exact `GL_FLOAT` equality. Log per attachment: pixels compared, components differing, max abs
  diff, first differing coord.
- **Verdicts (always one terminal line):** PASS (metadata matched, trustworthy, all outside-mask diffs
  zero) / FAIL (metadata matched, ≥1 outside-mask value differs) / INCONCLUSIVE (camera/view/viewport/
  layout/queue changed, readback/GL error, invalid mask, emissive layout changed, or scene not stable).
  In P0 also run a WHOLE-viewport diff (no mask) — since nothing draws, OFF==ON globally; still build the
  mask path so P1 reuses it unchanged.

## 6. Entry points + exact call site (llviewerdisplay.cpp, the main deferred branch)
```cpp
// pipeline.h:  void renderGhostDeferredOpaqueMasked(const LLCamera& camera);
// llactormover.h: void buildGhostDeferredQueue(const LLCamera&, U32 view_stamp);
//                 const GhostProxyQueue& getGhostDeferredQueue(const LLCamera&, U32 expected_view_stamp) const;

gGL.setColorMask(true, true);
LLCamera& camera = *LLViewerCamera::getInstance();
LLActorMover::instance().buildGhostDeferredQueue(camera, GHOST_VIEW_WORLD_MAIN);  // BEFORE deferred render
gPipeline.renderGeomDeferred(camera, true);
LLScopedGhostRenderInvariant invariant("renderGhostDeferredOpaqueMasked", camera,
    GHOST_VIEW_WORLD_MAIN, &LLActorMover::instance().ghostDeferredCounters().mInvariantViolations);
gPipeline.renderGhostDeferredOpaqueMasked(camera);
invariant.finish();
gPipeline.captureGhostDeferredContaminationSampleIfRequested(camera, GHOST_VIEW_WORLD_MAIN);
```
`view_stamp` stays internal to the pipeline impl initially (`constexpr U32 view_stamp = GHOST_VIEW_WORLD_MAIN`).
Build the queue AFTER harvest but BEFORE `renderGeomDeferred` (ideally right before/after the deferred
target bind/clear) — makes the mirror path naturally `build(mirror_cam)→renderGeomDeferred(mirror_cam)→
renderGhostDeferredOpaqueMasked(mirror_cam)`. Capture must run here, before texture-unit cleanup /
`rt.flush()` / SSAO/SSR/velocity / `renderDeferredLighting()`.

## New settings (app_settings/settings.xml)
`GhostDeferredDebugLog` (Bool, 0), `GhostDeferredContaminationTest` (S32, 0), `GhostDeferredDiffMaskPad`
(S32, 2). Small LLRender/LLCamera API adds: `LLRender::getMatrix(eMatrixMode)`, `LLCamera::AABBInFrustum`
const overload.

## What P0 proves / can't
Proves: moved harvest usable at the insertion point; queue records frame/view-local; cull+bounds run;
submission entry point inert; invariant checker detects injected state changes; readback/diff reaches
explicit verdicts. CANNOT validate (needs P1 to draw): shader/pass restoration after real draws,
palette-cache invalidation between clones, material/mask correctness, clone-only G-buffer changes,
two-placement transform correctness.
