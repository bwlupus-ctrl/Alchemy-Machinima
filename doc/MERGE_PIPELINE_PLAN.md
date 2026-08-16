# `pipeline.cpp` reapplication plan

## Decision

Use pinned upstream `pipeline.cpp/.h` as the base and reapply fork functionality as small,
feature-grouped patches. Do **not** reverse-integrate upstream's renderer hunks into the fork file.

Why:

- The fork file is not merely 15,000 lines larger; its custom blocks are interwoven with old texture,
  target, shader, depth, probe, shadow, PBR-alpha, and impostor contracts.
- Upstream's approximately 1,533 changed lines sit at architectural choke points: target allocation,
  shader binding, shadow/probe binding, post processing, draw dispatch, and impostor generation.
- A textually clean reverse integration could still compile against obsolete semantics and silently
  regress VCam/clones.

Risk layer: API-contract plus textual. Confidence: high.

## Rules for every reapplication patch

1. The upstream function body is the starting point.
2. Extract the smallest fork-owned behavior, not the whole surrounding function.
3. Translate old binds to `ALTextureSlot` and a named `ALSampler` at the call site.
4. Allocate immutable targets with sized formats/mip policy/depth format before binding.
5. Keep upstream reverse-Z, UBO, SH-probe, PBR, and impostor code unless the feature explicitly
   extends it.
6. Make every auxiliary/capture state change symmetric and locally restorable.
7. Land each group as a separate implementation commit with a named rollback point.

## Upstream anchor regions

These functions must remain upstream-led:

| Region | Pinned upstream anchor | Reason |
|---|---:|---|
| Deferred target creation | `pipeline.cpp:907-1102` | Immutable target allocation, depth format, new samplers |
| Postprocess target binds | `pipeline.cpp:7260-8976` | Named sampler and updated target semantics |
| Shadow binding | `pipeline.cpp:9090-9177` | PCF/PCSS raw-versus-compare sampler contract |
| Deferred target bind/unbind | `pipeline.cpp:9298-9365` | sRGB decode and point/mirror sampling |
| Reflection probe binding | `pipeline.cpp:10328-10347` | Radiance + SH coefficients + UBO |
| Main spot shadow projection | `pipeline.cpp:11808-11821` | `al_perspective` and reverse-Z bias |
| Impostor generation | `pipeline.cpp:12025-12463` | Correct coverage/material/normal bake |

## Grouped reapplication map

### P0. Types, settings cache, and lifecycle skeleton

Fork source: top-level statics, `pipeline.h` custom members, initialization and release functions.
Destination: corresponding upstream declarations plus constructor/init/cleanup blocks.

Reapply:

- Feature flags and cached controls for BDMerge, Prism, weather, projector volumetrics, sidecar, and
  10-bit rendering.
- Target/member declarations without allocating them yet.
- Scoped state structures for Prism auxiliary rendering.
- Custom render-type IDs only after reconciling upstream enum/layout changes.

Do not reapply:

- Raw sampler/texture-unit state caches.
- Removed probe irradiance textures or raw UBO handles.
- `sImpostorRenderAlphaDepthPass`.

Risk: textual/API-contract, high. Confidence: high.

### P1. Render-resource allocation and release

Fork source: resource creation near `pipeline.cpp:1339-1528`, main buffer creation near `:1800-2205`,
plus target-specific weather/froxel/projvol/sidecar/10-bit blocks.
Destination: upstream `createGLBuffers`, target-pack allocation, resize, and release functions around
`pipeline.cpp:907-1102` and their callers.

Reapply by target family:

1. Prism retained outputs (`mPrismLensOutput[]`) and feed routing.
2. Dedicated Prism spot maps (`mPrismSpotShadow[]`).
3. Velocity/motion targets and 10-bit post chain.
4. Visible-diffuse sidecar attachment/target.
5. Projector-volumetric and froxel targets.
6. Weather rain occlusion/surface/lightning targets.
7. Fork-only LUT/noise/dust resources.

For each target record color internal format, depth/stencil need, mip mode, resize owner, and sample
sampler in the patch description. Replace `setManualImage`/raw allocation with immutable
`allocateTexture2D`. Preserve upstream deletion/reallocation ordering.

Risk: API-contract, very high. Confidence: high.

### P2. Core frame routing and postprocessing

Fork source: custom target selection and bind helpers, velocity pass, sidecar routing, and 10-bit
composite around the fork's `renderGeomDeferred`, `renderGeomPostDeferred`, and post chain.
Destination: upstream functions of the same names after its sampler conversion.

Reapply:

- Target-routing decisions only; keep upstream bind calls and pass their named sampler.
- Motion/velocity render invocation and previous-frame bookkeeping after upstream matrix UBO updates.
- Sidecar MRT guard and publication path, preserving attachment limits.
- 10-bit/ReShade source/destination choice and final blit.
- Prism output/capture target routing where the feed substitutes an auxiliary screen/light target.

Do not copy fork postprocess loops wholesale. Upstream uses `PointMirror`, `BilinearMirror`, and
`BilinearClamp` deliberately (`pipeline.cpp:7260-8976`); preserve those choices.

Risk: textual/API-contract, high. Confidence: high.

### P3. Prism auxiliary state and reflection probes

Fork source:

- `beginPrismAuxiliaryState`: `pipeline.cpp:9338`
- `activatePrismAuxiliaryProbeState`: `:9408`
- `endPrismAuxiliaryState`: `:9501`
- fork probe binder: `:17746-17839`

Destination:

- Reapply scoped state functions near upstream camera/capture state helpers.
- Extend upstream `bindReflectionProbes` at `:10328`; never replace it.

Required adaptations:

- Stage/restore the full `ReflectionProbeData` through `ALUniformBuffer::update`; set staged
  `iterationCount=0.f` and `glossySampleCount=0.f`, because upstream moved both SSR controls into the
  block and the fork's old per-bind float uniforms no longer control them.
- Bind upstream `mSHCoeffs` with `ALSamplers::PointClamp`.
- Clamp feed probe count to one and suppress hero/SSR/temporal leakage only while
  `sPrismLensRender` is set. Replay the committed `prism_auxiliary` guards into upstream SH
  `reflectionProbeF.glsl` at both SSR branches and both hero-probe tap sites.
- Keep upstream normal rendering byte-identical.

Risk: API-contract, high. Confidence: high for structure, medium for PBR-flicker acceptance.

### P4. Main/projector/Prism shadows

Fork source:

- existing target selector `getSpotShadowTarget()` near `pipeline.cpp:18713`
- dedicated feed generator `:19762-20111`
- committed projector-light setup; cookie remapping is optional stash work
- expanded spot-shadow count and selection logic

Destination:

- upstream `bindShadowMaps` at `:9090`
- upstream `setupSpotLight` cookie matrix near `:10168`
- upstream main shadow rendering near `:11808`

Required adaptations:

- Adapt the existing `getSpotShadowTarget()` selector for the upstream target API, but route Prism
  and main targets through upstream sampler selection. Do not add a parallel selector.
- For shadow depth use `al_perspective`, upstream reverse-Z bias, and the exact depth-format policy
  from `allocateShadowBuffer()` (`DEPTH_FMT_32F` when the setting or reverse-Z requires it, otherwise
  `DEPTH_FMT_24`). Do not substitute `mainDepthFormat()`.
- Keep projector texture projection on forward `glm::perspective`. Cookie atlas/orientation uniforms
  are not present in `47af3df7b1a`; port them only as a separately accepted `stash@{0}` slice.
- Keep PCSS on raw depth and lower qualities on comparison sampling.
- Remove every target-level compare mutation.
- Preserve the fork's byte-identical-main gate: generating a feed map cannot mutate main shadow
  target contents, light selection, or sampler state.

Risk: API-contract/shader-drift, very high. Confidence: high.

### P5. BDMerge alpha and interleaving

Fork source:

- `postSort` bridge fan-out and alpha list ordering around `pipeline.cpp:4733-4890`
- `canUseInterleavedAlpha()` and `sortAlphaGroupsForInterleaving()`
- `lldrawpoolalpha.*`, avatar/control-avatar bridge stamps, and spatial comparators

Destination:

- upstream `postSort` and alpha-pool dispatch, keeping upstream material/sampler/impostor changes.

Reapply:

- `EAlphaStream`; both `LLSpatialGroup::mAvatarDepth` and `LLSpatialBridge::mAvatarDepth`; the
  `LLVOAvatar` and `LLControlAvatar` bridge producers; bridge-to-group fan-out; deterministic tie
  breaks; and the merged walk. Land both fields, both stamp producers, and the pipeline consumer
  together.
- Runtime precedence: eligible interleaved -> BDMerge attachment 3-pass -> stock two-pass.
- Sidecar guard, water rejection, attachment filters, and fallback behavior.

Adapt/remove:

- Remove `sImpostorRenderAlphaDepthPass` from the per-group depth precondition.
- Exclude or route impostor baking through the upstream coverage-aware path until explicitly
  validated.
- Keep upstream PBR/legacy alpha drawing inside each selected group.

The existing implementation is preserved in commit `597265b55d7`; use it as the feature reference,
not as a whole-file source.

Risk: textual/API-contract, high. Confidence: high for replay scope, medium for impostor interaction.

### P6. Clone/outer-transform pipeline hooks

Fork source: clone cull/movement/impostor hooks plus outer transform in draw-info/matrix code.
Destination: upstream cull/sort/draw and `generateImpostor` regions.

Reapply:

- Clone render-type eligibility, outer-transform-aware cull/LOD inputs, and movement invalidation.
- Draw-info transform separation and matrix-cache identity outside `pipeline.cpp` first.
- Minimum clone hook around upstream impostor generation.

Do not replace upstream `generateImpostor` (`:12025-12463`) or carry the removed extra depth pass.

Risk: API-contract, high. Confidence: medium-high.

### P7. Projector volumetrics and froxel renderer

Fork source: target creation plus the projector-volumetric/froxel feature blocks around
`pipeline.cpp:13503` and `:14180` onward (exact lines will move as preceding groups land). Do not use
`:12344` as a projector anchor; that region belongs to weather handling in the pinned fork.
Destination: upstream deferred-light preparation, light selection, shadow binding, and post-light
composite.

Reapply in four commits:

1. Targets/resources and feature settings.
2. Shader registration/features/UBOs.
3. Injection/integration/temporal passes with upstream depth reconstruction.
4. Composite/bloom feed and debug view.

Use upstream `deferredUtil` and `shadowUtil`. Mark custom depth comparisons with `REVERSE_Z` handling.
Projvol shadow sampling must share `bindShadowMaps`; do not own a second compare-state policy.

Risk: API-contract/shader-drift, high. Confidence: medium-high.

### P8. Weather renderer

Fork source: weather blocks around `pipeline.cpp:17901` onward plus resource allocation and shader
manager additions.
Destination: upstream deferred/post-deferred frame regions.

Reapply rain exposure/occlusion, surface, rain upsample, and lightning as separate patches. For rain
occlusion, specify whether depth is stored/reconstructed as reverse-Z and implement a matching branch
or matrix/compare convention. Bind noise/repeating textures with explicit wrap samplers.

Risk: shader-drift/API-contract, high. Confidence: medium until depth visualization.

### P9. Visible-diffuse sidecar and ReShade/10-bit integration

Fork source: sidecar publication in alpha and deferred paths, target guard, final output selection.
Destination: upstream MRT setup and postprocess end.

Reapply:

- The `publish_visible_diffuse` guard and indexed draw-buffer guard.
- Sidecar attachment only when the target has that attachment.
- Visible-diffuse seed pass and publication timing.
- 10-bit/ReShade final target selection and copy.

Never assume a fixed attachment index without checking the upstream target layout. Use named samplers
for source sampling and preserve upstream sRGB decode decisions.

Risk: API-contract, high. Confidence: medium-high.

### P10. Forced mask and remaining narrow render deltas

Fork source: `llvovolume.cpp:5914-5955`, draw-pool debug/feature hooks, dust, LUTs, and UI-controlled
minor paths.
Destination: upstream functions one small patch at a time.

Preserve the forced-mask `!gltf_mat` guard. Handle dust/LUT raw allocation under immutable storage.
Do not use this group as a catch-all for unexplained diff hunks: every remaining hunk needs a feature
owner or is dropped.

Risk: textual/API-contract, medium-high. Confidence: high.

## Hunk triage procedure

Before implementing a group, generate a zero-context diff from merge base to fork for only the
owned files and label every hunk:

- `KEEP-ADAPT`: feature behavior is required but calls an upstream-changed contract.
- `KEEP-AS-IS`: isolated fork logic whose dependencies are unchanged.
- `UPSTREAM-WINS`: stock code replaced by upstream architecture.
- `DROP-OBSOLETE`: old sampler, mutable allocation, old shader registration, or old impostor pass.
- `UNKNOWN`: stop that group until ownership/intent is identified.

No hunk may be silently omitted. Store this classification in the implementation commit message or
an accompanying checklist.

## Static checkpoints (no build required)

After each implementation group, use source searches to ensure:

- no re-ported custom block uses `getTexUnit`, texture-state filtering/addressing, or raw target
  allocation;
- no Prism/projvol block mutates shadow comparison parameters;
- no custom program is added to `mShaderList` or built with `make_rigged_variant`;
- no clone/interleaved code references `sImpostorRenderAlphaDepthPass`;
- no fork copy reintroduces `mIrradianceMaps` or raw `GLuint mUBO`;
- every custom clip/depth comparison has an explicit upstream depth-contract review;
- upstream shared shader and impostor commits remain present in the resulting history/tree.

## Rollback boundaries

Use one commit per vertical slice and create a checkpoint branch before each high-risk renderer
slice. A regression inside a clean, isolated slice can be reverted. Once later slices depend on its
types, fields, shader ABI, or resource ownership, a plain revert may not compile or may silently
break consumers: either fix forward or reset a disposable integration branch to the last checkpoint
and replay the later accepted slices. Never resolve a failing group by reverting an upstream
architecture commit.

## Concrete pipeline patch shapes

### Adapt the existing central target selector

The committed fork already has `LLPipeline::getSpotShadowTarget()` near `pipeline.cpp:18713`. Port
that function in place so upstream `bindShadowMaps()` remains the only sampler authority; the block
below is the intended adapted body, not a proposal for a new helper:

```cpp
LLRenderTarget* LLPipeline::getSpotShadowTarget(U32 index)
{
    if (index >= MAX_SPOT_SHADOWS)
    {
        return nullptr;
    }

    LLRenderTarget& target = sPrismLensRender
        ? mPrismSpotShadow[index]
        : mSpotShadow[index];
    return target.isComplete() ? &target : nullptr;
}
```

Adversarial review:

- Do not return a non-null target based only on width; a failed immutable allocation can leave
  dimensions without a complete framebuffer.
- The main binder currently loops a fixed number of spot units. If the fork raises
  `MAX_SPOT_SHADOWS`, shader uniform arrays, GLSL defines, UBO layout, and compare-unit masks must all
  be raised together or the helper indexes resources that no program can sample.

Risk: API-contract. Confidence: high for selection, medium for expanded count.

### Adapt the existing RAII state guard

The committed fork already defines `ScopedPrismRenderState` in `llprismlens.cpp` near `:4796` and
instantiates it in `renderAuxiliaryView()` near `:5523`. Adapt that guard to upstream; do not create
`ScopedPrismAuxiliaryState` or split restoration ownership. Preserve its existing `isValid()` flow:

```cpp
ScopedPrismRenderState scoped_state;
if (!scoped_state.isValid())
{
    registry.deferRetry(slot, 1);
    return;
}

// Every return below this line restores probe UBO, lights, matrices,
// shadow selections, viewport/target state, and temporal counters.
```

The guard already calls `beginPrismAuxiliaryState()` and pairs it with restoration. Extend its saved
state only where upstream introduces a new mutable dependency, and keep copy/move disabled.

Adversarial review:

- The guard must outlive every feed render and resolve/copy operation that uses staged state, but it
  must destruct before main `stateSort`.
- Copy/move must be deleted; two destructors restoring the same snapshot corrupt nesting.
- Nested auxiliary renders should be rejected by `beginPrismAuxiliaryState()` unless the state
  object is explicitly made stack-based.

Risk: API-contract. Confidence: high.

### Immutable shadow resource helper

Do not duplicate mutable allocation in Prism/weather/projector code. A helper can encode upstream's
depth policy:

```cpp
bool LLPipeline::allocateDepthOnlyTarget(LLRenderTarget& target,
                                         U32 width,
                                         U32 height,
                                         LLRenderTarget::eDepthFormat depth_format)
{
    if (target.isComplete() &&
        target.getWidth() == width &&
        target.getHeight() == height &&
        target.getDepthFormat() == depth_format)
    {
        return true;
    }

    target.release();
    return target.allocate(width,
                           height,
                           0,
                           true,
                           false,
                           ALTextureSlot::TT_TEXTURE,
                           LLRenderTarget::MIPS_NONE,
                           depth_format);
}
```

Only call this from a lifecycle point where `target` is known not to be bound or present in the RT
stack.

Adversarial review:

- A generic helper can tempt callers to release a live target mid-pass. Keep it private and call it
  only from allocation/quality-change stages.
- `getDepthFormat()` is meaningful only for a target with depth. Check completeness first as shown.
- Weather occlusion may require a color-encoded linear depth target rather than depth-only. Do not
  force it through this helper without settling its shader contract.

Risk: API-contract. Confidence: high.

### Bridge-to-alpha-group fan-out

The interleaved patch belongs before the alpha draw-map lookup in upstream `postSort()`:

```cpp
LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();
if (bridge && bridge->mAvatarp)
{
    group->mAvatarp = bridge->mAvatarp;
    group->mRenderOrder = bridge->mRenderOrder;
    group->mAvatarDepth = bridge->mAvatarDepth;
}

auto alpha = group->mDrawMap.find(LLRenderPass::PASS_ALPHA);
```

Do not alter upstream's draw-info/material population around this insertion. The later
`sortAlphaGroupsForInterleaving()` changes ordering only for the eligible consumer.

Adversarial review:

- A group reused after an attachment detaches can retain the previous avatar stamp unless the bridge
  or group fields are cleared by lifecycle code. Test detach/reattach within one frame.
- If two control avatars share a bridge unexpectedly, the last stamp wins; assert ownership in debug
  builds or log a conflict.
- Pointer address tie breaks must use `std::less<const LLVOAvatar*>`, not relational comparison on
  unrelated pointers.

Risk: API-contract. Confidence: medium-high.

### Sidecar draw-buffer guard

The fork guard must stay scoped around only programs that declare the sidecar output:

```cpp
const bool publish_visible_diffuse =
    sidecar_target &&
    sidecar_target->getNumTextures() > SL_SIDECAR_ATTACHMENT &&
    LLRender::indexedDrawBufferGuardSupported();

static const U32 guarded_attachments[] = {
    SL_SIDECAR_ATTACHMENT,
    SL_COVERAGE_ATTACHMENT
};
LLScopedIndexedDrawBufferGuard sidecar_guard(
    publish_visible_diffuse,
    guarded_attachments,
    LL_ARRAY_SIZE(guarded_attachments));
```

Re-port `LLScopedIndexedDrawBufferGuard` and its `LLRender` support as one dependency if upstream does
not already contain the fork extension. The destructor must restore the full attachment state.

Adversarial review:

- Counting attachments is not enough if the sidecar index moved; its semantic index must be shared by
  allocation, shader output, and publication.
- Leaving the narrowed draw-buffer mask active corrupts every following deferred pass.
- A fragment shader that does not declare the sidecar output must never draw with that attachment
  enabled merely because the FBO owns it.

Risk: API-contract. Confidence: medium until final upstream guard API is selected.

## Adversarial hunk ledger

Every implementation commit should attach a ledger like this, with real line anchors filled in:

| Fork hunk/function | Owner | Classification | Upstream destination | Contract translation | Negative test |
|---|---|---|---|---|---|
| `allocatePrismLensOutput` | VCam | KEEP-ADAPT | target lifecycle | full immutable signature; fixed capacity | resize while sibling samples |
| Prism raw UBO upload | VCam | KEEP-ADAPT | probe state scope | full-struct `ALUniformBuffer::update` | forced early return |
| Prism `glTexParameteri` | VCam | DROP-OBSOLETE | `bindShadowMaps` | named sampler selected by compiled tier | PCF/PCSS runtime toggle |
| `sImpostorRenderAlphaDepthPass` | alpha/clones | DROP-OBSOLETE | none | upstream coverage bake owns order | detailed/impostor boundary |
| `resolve_outer_transform` | clones | KEEP-ADAPT | upstream draw-info construction | transform in batch key and matrix cache | two adjacent differently scaled clones |
| shared `shadowUtil` fork diff | projvol | UPSTREAM-WINS plus narrow adapter | upstream shared shader | call upstream shadow API | raw/compare sampler mismatch |

The reviewer should reject a commit whose ledger says only "conflict resolved" or "ported pipeline
changes". It must name feature ownership and the changed contract.

## Static adversarial source checks

Run these against the integration branch after the relevant groups land:

```powershell
rg -n "getTexUnit|setTextureFilteringOption|setTextureAddressMode" `
  indra/newview/llprismlens.cpp indra/newview/pipeline.cpp

rg -n "glTexParameteri.*COMPARE|glBufferSubData.*UNIFORM_BUFFER" `
  indra/newview/pipeline.cpp

rg -n "mShaderList|make_rigged_variant" `
  indra/newview/llviewershadermgr.cpp

rg -n -C 2 "finalizeShaderList|no_redundant_shaders" `
  indra/newview/llviewershadermgr.cpp

rg -n "sImpostorRenderAlphaDepthPass|mIrradianceMaps|getObjectSkinnedTransform" `
  indra/newview indra/llrender
```

The first, second, third, and fifth commands are rejection searches. The finalizer search is a
positive structural check: upstream's assertion-only shell and `no_redundant_shaders()` assertion
must remain, while the old fork `mShaderList` body must not. These checks are not proof of
correctness; runtime attack cases in the subsystem specifications still apply.
