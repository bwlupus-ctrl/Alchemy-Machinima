# Solid Completion Rescue — Implementation Recipe (Codex, 2026-07-23)

Concrete HOW for the slice designed in SCENE_LIT_CLONE_SOLID_RESCUE_BLUEPRINT.md. Two-hook early-
testing layout, structured so the later pool-boundary dispatcher needs no rewrite. Codex session
019f8cc2. IMPLEMENT then MANDATORY Codex review -> 0 must-fix.

## pipeline.h
Public (near the ghost entry point):
```cpp
enum class EGhostForwardStage : U8 { FULLBRIGHT_OPAQUE, FULLBRIGHT_SHINY, FULLBRIGHT_MASKED, FINALIZE };
void renderGhostPostDeferred(const LLCamera& camera, EGhostForwardStage stage);
```
Private (near mGhostDeferredCoverage):
```cpp
struct GhostCategoryProgress { U32 mRequired=0,mDrawn=0; bool mClassified=false,mFailed=false,mFinalized=false; };
enum EGhostForwardSolidStageMask : U32 { GHOST_FORWARD_SOLID_NONE=0, GHOST_FORWARD_SOLID_FULLBRIGHT=1u<<0,
    GHOST_FORWARD_SOLID_SHINY=1u<<1, GHOST_FORWARD_SOLID_MASKED=1u<<2 };
struct GhostSubmissionProgress {
    GhostCategoryProgress mRiggedSolid, mRiggedBlend;
    U32 mExpectedForwardSolidStages=GHOST_FORWARD_SOLID_NONE, mCompletedForwardSolidStages=GHOST_FORWARD_SOLID_NONE;
    bool mAnyDrawSubmitted=false; };   // preserves mProxiesSubmitted ">=1 draw in ANY phase"
U32 mGhostSubmissionProgressFrame=0; std::map<LLUUID,GhostSubmissionProgress> mGhostSubmissionProgress;
void finalizeGhostRiggedSolidCoverage(const LLActorMover::GhostProxyQueue& queue);
```
Keep mGhostDeferredCoverage(Frame) separate; NEVER use it as pending storage.

## Queue guard (llactormover) — non-asserting probe
```cpp
bool LLActorMover::hasValidGhostDeferredQueueThisFrame(const LLCamera& c, U32 view) const {
    return mGhostDeferredQueue.mFrameStamp==LLFrameTimer::getFrameCount()
        && mGhostDeferredQueue.mViewStamp==view && mGhostDeferredQueue.mCameraIdentity==&c; }
```
No warn/assert/mStaleQueueSkips. Strict accessor unchanged.

## renderGhostPostDeferred guards (top, IN ORDER)
```cpp
if (gCubeSnapshot || LLPipeline::sReflectionRender || LLPipeline::sRenderingHUDs
    || LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD) return;
if (mGhostSubmissionProgressFrame != LLFrameTimer::getFrameCount()) return;
LLActorMover& mover = LLActorMover::instance();
if (!mover.hasValidGhostDeferredQueueThisFrame(camera, GHOST_VIEW_WORLD_MAIN)) return;
const auto& queue = mover.getGhostDeferredQueue(camera, GHOST_VIEW_WORLD_MAIN);
```
gCubeSnapshot ALONE is insufficient (same LLViewerCamera addr can pass pointer/frame check in a cube
render -> explicit view guard needed). FINALIZE stage: after guards, call
finalizeGhostRiggedSolidCoverage(queue); return; (no GL work).

## Hook in renderDeferredLighting (~pipeline.cpp:13243), inside the pushRenderTypeMask block:
```cpp
{ LLScopedGhostRenderInvariant inv("renderGhostPostDeferredSolids",
      &LLActorMover::instance().ghostDeferredCounters().mInvariantViolations);
  const LLCamera& cam = *LLViewerCamera::getInstance();
  renderGhostPostDeferred(cam, EGhostForwardStage::FULLBRIGHT_OPAQUE);
  renderGhostPostDeferred(cam, EGhostForwardStage::FULLBRIGHT_SHINY);
  renderGhostPostDeferred(cam, EGhostForwardStage::FULLBRIGHT_MASKED);
  inv.finish(); }
renderGeomPostDeferred(*LLViewerCamera::getInstance());
popRenderTypeMask();
renderGhostPostDeferred(*LLViewerCamera::getInstance(), EGhostForwardStage::FINALIZE);
```
Solids INSIDE the mask scope (manual draws ignore cull maps; keeps association). FINALIZE after pop
(bookkeeping). Move emitGhostDeferredDebug() from the G-buffer site to AFTER renderDeferredLighting()
in llviewerdisplay.cpp (else forward draw/finalize counts missing). Contamination test stays put.

## Invariant tracked set (so correct shiny setup reads clean): FBO, viewport/scissor, modelview +
## tex matrices 0-3, color mask, depth enable/write/func, blend enable/factors/eq, cull, FB sRGB,
## GL program + viewer shader ptr, active tex unit, array/element buffers, gGLLastMatrix. NOT
## individual texture bindings -> must still unbind exposure/env/diffuse/probe explicitly. Capture +
## restore blend FACTORS manually (LLGLEnable/Disable restore enable only).

## classifyGhostBatchPhase(pass) (pipeline.cpp anon ns) — SINGLE execution-domain classifier
GBUFFER_SOLID: SIMPLE_RIGGED, ALPHA_MASK_RIGGED, BUMP_RIGGED, SHINY_RIGGED, GLTF_PBR_RIGGED,
  GLTF_PBR_ALPHA_MASK_RIGGED, and default-if ghostMaterialShaderIndex(pass)>=0.
FORWARD_FULLBRIGHT: FULLBRIGHT_RIGGED. FORWARD_FULLBRIGHT_SHINY: FULLBRIGHT_SHINY_RIGGED.
FORWARD_FULLBRIGHT_MASKED: FULLBRIGHT_ALPHA_MASK_RIGGED. else ghost_pass_is_blend->BLEND,
ghost_pass_is_glow->GLOW, else UNSUPPORTED_SOLID.

## renderGhostDeferredOpaqueMasked restructure
Reset BOTH maps at same point (frame-stamp). Per proxy: GhostSubmissionProgress& s=map[id];
s.mRiggedSolid.mClassified=true. Per batch classify FIRST:
- BLEND/GLOW: continue (not solid).
- FORWARD_*: ++solid.mRequired; s.mExpectedForwardSolidStages |= bit; continue (pending, NOT fail).
- UNSUPPORTED_SOLID: ++mRequired; mFailed=true; continue.
- null mInfo on a solid: ++mRequired; mFailed=true; continue.
- GBUFFER_SOLID: ++mRequired; proceed to shader/validate/draw.
Any post-classification failure: mFailed=true; continue. Success: ++mDrawn; ++mActualDrawCalls;
++mRiggedSolidDrawCalls; if(!mAnyDrawSubmitted){mAnyDrawSubmitted=true; ++mProxiesSubmitted;}.
REMOVE the old any_solid_draw/all_solid_ok + RIGGED_SOLID set here.
New switch cases (before GLTF/default):
  PASS_BUMP_RIGGED -> shader=gDeferredBumpProgram.mRiggedVariant; bump_deferred=true;
  PASS_SHINY_RIGGED -> shader=gDeferredDiffuseProgram.mRiggedVariant; legacy_textured=true; shiny_normalized=true;
Bump per-batch (own branch, NOT legacy_textured): setMinimumAlpha(cutoff);
  if(!LLDrawPoolBump::bindBumpMap(*di,bump_map_channel)){mFailed;continue;} drew=pushGhostBumpBatch(*di,bump_diffuse_channel);
Bump shader-switch: enable DIFFUSE_MAP+BUMP_MAP channels (per-batch bind simplest+safest); if channel<0 -> fail.
  Teardown: disableTexture DIFFUSE_MAP+BUMP_MAP, restore active unit.

## finalizeGhostRiggedSolidCoverage(queue): per proxy, find progress; skip if mFinalized; mFinalized=true;
stages_complete = (mCompleted & mExpected)==mExpected; complete = mClassified && !mFailed &&
mRequired>0 && mDrawn==mRequired && stages_complete; if complete set GHOST_COVERAGE_RIGGED_SOLID
(0->1: ++mRiggedSolidInstancesSubmitted). NEVER clear a bit.

## Forward dispatcher skeleton: after guards+FINALIZE, stage_bit=ghostForwardStageBit(stage);
early-out if no progress entry has mExpectedForwardSolidStages & stage_bit. Save active unit/blend
factors/color mask; establish per-stage shader; iterate queue proxies (ScopedGhostTransform each);
filter batches by classifyGhostBatchPhase==this stage; validate/upload/draw/account (mDrawn/mFailed,
NOT mRequired again); AFTER stage, for every visible proxy progress that expected this stage:
mCompletedForwardSolidStages |= stage_bit (even if batches failed; NOT for culled/absent/unexpected).
Teardown + restore.

## Forward per-batch validate/account (all stages)
null/VB/count/avatar/skin -> mFailed;continue. !shader||!isComplete -> mFailed;continue.
!uploadMatrixPalette(*di) -> mFailed;continue. !push_helper -> mFailed;continue. else ++mDrawn +counters.

## FULLBRIGHT_OPAQUE: shader=gDeferredFullbrightProgram.mRiggedVariant. depth(TRUE,TRUE,LEQUAL),
blend OFF, cull ON, setColorMask(true,false). bind; exposure=enableTexture(EXPOSURE_MAP)>=0 ->
bind &mExposureMap. Per batch pushGhostBatch(*di,true). Teardown disableTexture(EXPOSURE_MAP);unbind.

## FULLBRIGHT_MASKED: gDeferredFullbrightAlphaMaskProgram.mRiggedVariant, same state. Per batch
setMinimumAlpha(cutoff) then pushGhostBatch(*di,true). Same exposure probe.

## FULLBRIGHT_SHINY (hard): local vars only, DO NOT touch file-statics shiny/cube_channel/
diffuse_channel/sVertexMask/mShaderLevel. shiny_shader=gDeferredFullbrightShinyProgram.mRiggedVariant.
BEGIN ONCE per stage (not per clone). SHINY_ORIGIN + env matrix from CAMERA modelview BEFORE any
ScopedGhostTransform:
```cpp
gGL.matrixMode(MM_MODELVIEW); gGLLastMatrix=nullptr; gGL.loadMatrix(gGLModelView); gGL.syncMatrices();
shiny_shader->bind();
LLMatrix4 mv; mv.initRows(V4(gGLModelView+0),+4,+8,+12);
LLVector3 so=LLVector3(gShinyOrigin)*mv; LLVector4 so4(so,gShinyOrigin.mV[3]);
shiny_shader->uniform4fv(SHINY_ORIGIN,1,so4.mV);
exposure=enableTexture(EXPOSURE_MAP)>=0 -> bind &mExposureMap;
if (sReflectionProbesEnabled) bindReflectionProbes(*shiny_shader);   // also does setEnvMat, camera-space
else if (cube_map=gSky.mVOSkyp?getCubeMap():null) { env=enableTexture(ENVIRONMENT_MAP,TT_CUBE_MAP);
    diff=enableTexture(DIFFUSE_MAP); cube_map->enableTexture(env); bind(env,cube_map); setEnvMat(*shiny_shader); }
```
State: depth(TRUE,TRUE,LEQUAL), blend ON, cull ON, BT_ALPHA, setColorMask(true,false). Diffuse: indexed
if di->mTextureList.size()>1 && sIndexedTextureChannels>1 (bind list to units, use tex-index attr,
NO scalar); else bind di->mTexture to diffuse_channel (diffuse_channel<0 -> fail). Draw helper
pushGhostFullbrightShinyBatch(params,diffuse_channel,indexed): validate; bind indexed/scalar; apply
mTextureMatrix on diffuse unit; drawRange; restore identity; true after draw. DO NOT call pushBumpBatch.
TEARDOWN (always, incl probe path -- do NOT copy stock's cube-only unbind): exit transforms; restore
camera modelview+sync; unbindReflectionProbes OR cube_map->disable()+restoreMatrix()+disableTexture
(ENVIRONMENT_MAP,TT_CUBE_MAP); disable exposure; disable diffuse/env; unbind shader; unbind VB; restore
tex matrices; restore active unit; restore blend factors/color mask/gGLLastMatrix.

## pushGhostBumpBatch(params,diffuse_channel): validate count/VB/channel; bind mTexture or unbind;
if mTextureMatrix -> activate diffuse unit, MM_TEXTURE, loadMatrix once, MM_MODELVIEW, flag; setBuffer;
drawRange; restore identity if flagged; return true. NO applyModelMatrix, NO file-statics.

## Depth: mRT->deferredScreen.shareDepthBuffer(mRT->screen) -> at the hook screen bound, lighting done,
shared opaque depth available, flush not yet run. LEQUAL matches clone G-buffer pixels; solid depth
writes extend the buffer for later stock alpha. Depth writes ON correct + necessary.

## Pitfalls (ranked): 1 premature RIGGED_SOLID (only FINALIZE sets it). 2 cube/probe queue reuse (view
guard + non-asserting probe). 3 SHINY_ORIGIN/envmat must be camera-modelview pre-transform. 4 don't
double-count mRequired in forward. 5 mark stage complete even after failure. 6 debug log moved late.
7 no file-static bump/shiny reuse. 8 always unbindReflectionProbes + unbind shiny shader. 9 texture
leak invisible to invariant -> explicit unbind. 10 restore blend factors (RAII enable != factors).
