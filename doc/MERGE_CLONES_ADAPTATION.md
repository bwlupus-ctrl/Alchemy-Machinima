# Entity clone / ghost adaptation specification

Base assumptions: fork `47af3df7b1a`, upstream `af0f3bd1beb`, merge base `7c11d3f38bd`.
The clone-owned files have no upstream textual churn, but their avatar, draw, impostor, skinning,
probe, and shader dependencies do.

## Direct answers

### 5. Impostor rewrite and clone parity

Yes, T7 changes the rendering surface the clone fixes build on. Upstream's two impostor commits
(`cda2d4fb0c1`, `0ef9c3d7f53`) correct material sorting and normals, accumulate real coverage in
alpha, stamp opaque coverage before forward alpha, and remove the old third depth-only scene render.
The fork's `LLPipeline::sImpostorRenderAlphaDepthPass` is absent upstream and must stay removed.

Port rule: upstream `LLPipeline::generateImpostor()` (`pipeline.cpp:12025`) and
`LLVOAvatar::renderImpostor()` are authoritative. Reapply only clone identity/eligibility, outer
transform, attachment grouping, and GhostDeferred hooks. Adapt interleaved alpha around upstream's
coverage pass; do not paste the fork's old impostor sequence.

The attach-float/impostor-gap fixes cannot be declared visually proven by static inspection. They
must be retested at the detailed-to-impostor boundary with rigged, unrigged, alpha, PBR, and animesh
attachments.

Risk: API-contract, high. Confidence: high for the required code direction; medium for visual parity.

### 6. Avatar-local skinning and 100x/150x outer scale

The outer render transform can compose correctly with upstream avatar-local skinning
(`aff85c50ddb`), but the fork's shader helpers cannot be retained.

Upstream represents a skinned point as a local blended term plus `skin_origin` and applies the active
model-view through `skinTransformH()`. Fork `LLRenderPass::applyModelMatrix()` applies
`LLClientOuterTransform` before the draw's model matrix (`lldrawpool.cpp:748-769`). Therefore:

- the local skin term receives outer scale/rotation;
- `skin_origin` receives the outer pivot translation through the same model-view;
- the transform is applied once, not twice, provided `skin_origin` is uploaded in untransformed
  avatar space and matrix synchronization occurs after `applyModelMatrix()`.

Preserve `resolve_outer_transform()` (`llvovolume.cpp:5512`) and `LLDrawInfo::mOuterTransform`
assignment (`:5838`) as narrow additions to the upstream draw-info construction. Validate 1x, 100x,
and 150x clones, including rigged attachments and animesh.

Risk: API-contract, medium-high. Confidence: high analytically, medium until runtime validation.

### 7. `LLVOAvatar` churn versus clone hooks

The upstream diff does not alter `idleUpdateMisc`, `updateRootPositionAndRotation`, or the clone's
hover/bounds behavior inside those functions. It also retains the animation maps
`mSignaledAnimations`, `mPlayingAnimations`, and `mAnimationSources` (upstream
`llvoavatar.h:1104-1109`). The upstream change to `processAnimationStateChanges` is profiling-only.

Upstream does change `renderImpostor`, `updateSkinInfoMatrixPalette`, visual-complexity accounting,
and attachment complexity/lifecycle regions. Reapply clone hooks to unchanged functions by context;
adapt palette/impostor code semantically; do not replace upstream complexity code.

Fork anchors: `llvoavatar.cpp:3105` (`idleUpdateMisc`), `:4878`
(`updateRootPositionAndRotation`), and `llghostavatar.cpp:1982-2068` (hold-on-source-change).

Risk: textual/API-contract, medium. Confidence: high.

### 8. Actor-ghost shader and vertex-alpha path

`actorghostF.glsl` keeps its feature-owned `ghostUseVertexAlpha` path and can survive. Its C++ writes
already explicitly set the control for rigged and static draws (`llactormover.cpp:4780,4913,5230`).
Preserve those writes after shader registration changes. The hashed uniform setter safely ignores a
missing location; upstream `hasUniform()` accepts a reserved `U32` index, not a hashed name.

`actorghostV.glsl` does **not** survive unchanged: it declares/calls the removed
`getObjectSkinnedTransform()` at `:44,55`. Rewrite the rigged branch to use upstream
`getSkinBlend()`, `skinTransformH()` for position, and `skinDirection()` for normals. Build the
rigged program with `gActorGhostProgram.createShader(LLGLSLShader::VARIANT_RIGGED)` and consume
`gActorGhostProgram.mRiggedVariant`; remove the cpp-local `gSkinnedActorGhostProgram` definition and
references. It is not declared in the header.

Risk: shader-drift/API-contract, high. Confidence: high.

## Per-file adaptation map

### `indra/newview/llghostavatar.cpp/.h`

Classification: clone logic `unchanged`; render integration `validate`.

- Reapply the complete class rather than mixing it into upstream avatar code; upstream has zero
  churn in these files.
- Preserve appearance/attachment cloning, MIRROR/DIRECTED/FROZEN modes, per-linkset control avatars,
  source-change retention, clone cleanup, and outer transform ownership.
- Preserve `GhostMirrorHoldOnSourceChange` at `llghostavatar.cpp:1982-2068`. The source
  `mSignaledAnimations` map remains available.
- Preserve `updateEntityOuterTransform` around `:292-370`; it is the source of the post-skin scale
  and foot-pivot matrix.
- Re-audit every direct target/texture/shader bind used by GhostDeferred or capture paths against
  named samplers and upstream program lifecycle.
- Do not duplicate upstream avatar complexity or impostor state. Call the adapted base behavior.

Risk: API-contract, medium-high. Confidence: high.

### `indra/newview/llcontrolavatar.cpp/.h`

Classification: class logic `unchanged`; draw stamps `semantic`.

- Upstream has no direct file changes. Preserve per-linkset animesh animation reconstruction and
  the `mSignaledAnimations` rebuild at `llcontrolavatar.cpp:595-640`.
- Preserve the interleaved-alpha bridge stamp at `:381-391`, including the fresh drawable bridge
  lookup and worn-animesh exclusion.
- Revalidate bridge lifetime and stamp clearing after upstream cull/post-sort changes; do not rely
  on a stale `mControlAVBridge`.
- Ensure animesh impostor participation follows upstream's coverage/material passes.

Risk: API-contract, medium. Confidence: high for source compatibility, medium for render ordering.

### `indra/newview/llvoavatar.cpp/.h`

Classification: mixed; port function by function.

- Start with upstream.
- Reapply clone-specific early-outs/adjustments to `idleUpdateMisc` (upstream about `:2998`) and
  `updateRootPositionAndRotation` (upstream about `:4742`) using contextual patches.
- Preserve the fork's attachment bridge stamping and `calcRiggedAlphaDepth()`
  (`fork :3090-3150`) for interleaved alpha, but fit it around upstream attachment processing.
- Reapply `markMoved` behavior only at the narrow clone/attachment call sites; do not replace
  upstream movement or complexity loops.
- Keep upstream `renderImpostor()` and its new material/coverage semantics, then add the minimum
  clone identity, attachment, outer-transform, and GhostDeferred hooks.
- Keep upstream `updateSkinInfoMatrixPalette()` as the base. Its palette begins with a four-float
  origin and stores rebased translations. Extend it only for the fork's previous-frame velocity
  palette as described below.
- Preserve the animation maps and reapply mirror reads. No member rename is required.

Risk: textual/API-contract, high in palette/impostor regions; medium elsewhere. Confidence: high.

### `indra/newview/llvovolume.cpp/.h`

Classification: `semantic` draw-info replay.

- Start with upstream `registerFace()` and its GLTF/material/sampler behavior.
- Reapply `resolve_outer_transform()` from fork `:5512-5534`.
- Reapply outer-transform-aware draw-info reuse comparisons (`:5712`, `:5793`) and assignment
  (`:5838`). This prevents batches with different clone transforms from coalescing.
- Reapply scale-aware pixel-area, cull, and LOD calculations, preserving upstream bounds/material
  logic.
- Reapply forced mask cutoff only after upstream initializes draw-info cutoff and only when
  `!gltf_mat` (`fork :5914-5955`).
- Revalidate attachment movement invalidation where the fork calls `gPipeline.markMoved`; do not
  copy obsolete target or sampler code surrounding those hooks.

Risk: textual/API-contract, high. Confidence: high.

### `indra/newview/lldrawpool.cpp/.h` and draw-info structs

Classification: `semantic` matrix integration.

- Reapply `LLClientOuterTransform` storage on `LLDrawInfo`/spatial structures.
- In upstream `LLRenderPass::applyModelMatrix`, include outer pointer and revision in the matrix-cache
  key, load the base model-view, multiply the enabled outer matrix, then multiply the draw model
  matrix. Preserve upstream shader-matrix dirty/update behavior.
- Translate any adjacent texture binding to `ALTextureSlot` with named samplers; do not take the
  fork function wholesale.
- Reset cached outer pointer/revision wherever upstream resets the model-matrix cache.

Risk: API-contract, high. Confidence: high.

### `indra/newview/lldrawable.cpp`, `llface.cpp`, `llviewerobject.*`, and
`llclientoutertransform.h`

Classification: fork data model `unchanged`; upstream integration `mechanical/semantic`.

- Reapply `LLClientOuterTransform` ownership on viewer objects and avatars.
- Reapply scale-aware bridge bounds in `lldrawable.cpp` and face geometry invalidation in
  `llface.cpp`; keep upstream geometry and sampler changes around them.
- Preserve reference counting and the revision counter. Matrix-cache invalidation depends on
  revision changes, not only pointer changes.
- Verify transformed bounds are used for culling/LOD while native geometry stays unmodified.

Risk: API-contract, medium-high. Confidence: high.

### `indra/newview/lldrawpoolalpha.cpp/.h`, `llspatialpartition.*`, and `pipeline.cpp`

Classification: `semantic` around upstream impostors.

- Reapply `EAlphaStream`, both `LLSpatialGroup::mAvatarDepth` and
  `LLSpatialBridge::mAvatarDepth` (fork `llspatialpartition.h:475,590`), bridge stamps from
  `LLVOAvatar` and `LLControlAvatar` (fork
  `llvoavatar.cpp:3194`, `llcontrolavatar.cpp:391`), and the pipeline fan-out (fork
  `pipeline.cpp:4827`) in one vertical slice. The field without both producers and its consumer is
  dead state; the consumer without the field does not compile.
- Preserve `BDMergeAlphaAttachmentSort` as the fallback and the sidecar draw-buffer guard.
- Remove all dependence on `sImpostorRenderAlphaDepthPass`. In an impostor bake, use upstream's
  legacy/coverage route unless a dedicated test proves the merged route preserves coverage.
- Keep upstream alpha/PBR material binding and named samplers inside the group walk.
- Revalidate rigged/unrigged/animesh groups draining as one avatar ensemble.

Risk: API-contract, high. Confidence: high for the exclusion; medium for the adapted gate.

### `indra/newview/llviewershadermgr.cpp/.h`

Classification: `semantic` lifecycle.

- Define only `gActorGhostProgram`; delete the cpp-local `gSkinnedActorGhostProgram` definition and
  references. Do not search for or invent a header declaration.
- Load base sources, call `createShader(VARIANT_RIGGED)`, and then use the base plus
  `mRiggedVariant` as `llactormover.cpp:4693-4701` already expects.
- Do not re-port `make_rigged_variant()` or the fork-populated `finalizeShaderList()` body. Preserve
  upstream's assertion-only `finalizeShaderList()` shell and its
  `llassert(no_redundant_shaders())` check.
- Retain live hashed/custom uniforms for actor ghost. Use upstream feature flags for matrices and
  rigging.

Risk: API-contract/shader-drift, high. Confidence: high.

### `indra/newview/app_settings/shaders/class1/interface/actorghostV.glsl`

Classification: mandatory `semantic` rewrite.

Conceptual rigged branch:

```glsl
mat3x4 blend = getSkinBlend();
vec4 view_position = skinTransformH(blend, position, modelview_matrix);
vec3 view_normal = normalize(mat3(modelview_matrix) * skinDirection(blend, normal));
```

Use the exact upstream helper signatures/includes at implementation time. Preserve the non-rigged
path, ghost placement, rim/distortion outputs, and vertex alpha.

Risk: shader-drift, high. Confidence: high.

### `actorghostF.glsl` and `llactormover.cpp`

Classification: mostly `unchanged` with lifecycle validation.

- Preserve `ghostUseVertexAlpha` and the alpha multiplication at shader `:251`.
- Preserve explicit zero/nonzero uploads at `llactormover.cpp:4780,4913,5230`; reset after filtered
  batches so state cannot leak.
- Continue choosing `gActorGhostProgram.mRiggedVariant` and the base program for static faces
  (`llactormover.cpp:4693-4701`).
- For hashed custom uniforms, rely on the setter's missing-location no-op; use `hasUniform()` only for
  reserved indexed uniforms.

Risk: shader-drift, medium. Confidence: high.

### Velocity shaders and palette upload

Classification: mandatory `semantic` redesign.

The fork's `getLastObjectSkinnedTransform()` cannot be copied onto upstream. Add a parallel previous
contract:

1. Store/upload `last_skin_origin`.
2. Prepend that origin to the previous palette buffer.
3. Subtract it from every previous palette translation on CPU, matching upstream's current-palette
   rebasing.
4. Implement `getLastSkinBlend()` returning `mat3x4`.
5. Compute prior position with the previous blend and the appropriate previous model-view transform.

Rewrite `velocityV.glsl` and `velocityAlphaV.glsl` to use current/previous blend helpers. Keep
`avatarVelocityV.glsl` separate: it uses the classic avatar palette layout, but still needs the new
matrix UBO interface audited.

Risk: API-contract/shader-drift, very high. Confidence: high for the design, medium until motion
vectors are visualized.

## Landing order inside the clone subsystem

1. Add outer-transform data types/ownership and matrix-cache composition.
2. Port scale-aware bounds, culling, LOD, and draw-info batch separation.
3. Take upstream avatar-local current palette; add the rebased previous-palette extension.
4. Port actor-ghost and velocity shaders through upstream variants/lifecycle.
5. Reapply `LLGhostAvatar` and `LLControlAvatar` behavior and animation mirroring.
6. Reapply bridge/avatar-depth stamps and interleaved alpha around upstream impostors.
7. Add only narrow GhostDeferred/impostor hooks to upstream's bake.

Each step should be a separate commit during implementation so it can be reverted without removing
the rest of the clone system.

## Acceptance checklist (later build/test phase)

- Appearance parity: body, BOM, legacy materials, PBR/GLTF, rigged and unrigged attachments.
- Animation modes: MIRROR, DIRECTED, FROZEN, source change with hold enabled and disabled.
- Per-linkset animesh has independent motion and no duplicate/stale bridge stamp.
- Scale: 1x, 100x, 150x; feet stay planted; rigged mesh, static attachments, and cull/LOD agree.
- Actor-ghost solid/alpha/wire/distortion styles; `ghostUseVertexAlpha` changes only intended draws.
- Velocity buffer visualization shows current/previous rigged positions without altitude jitter.
- Detailed-to-impostor transitions have no attachment float, coverage gap, missing PBR, or LOD flicker.
- Interleaved alpha composites clone hair/attachments against world alpha while legacy and BDMerge
  fallbacks remain available.

Overall clone port confidence: high for the dependency map, medium until scale, motion-vector, and
impostor acceptance tests are run.

## Concrete clone port code

### Actor-ghost rigged vertex conversion

Replace the old absolute transform declaration/call in `actorghostV.glsl` with upstream avatar-local
helpers:

```glsl
#ifdef HAS_SKIN
mat3x4 getSkinBlend();
vec3 skinDirection(mat3x4 blend, vec3 direction);
vec4 skinTransformH(mat3x4 blend, vec3 position, mat4 transform);
#endif

void main()
{
#ifdef HAS_SKIN
    mat3x4 blend = getSkinBlend();
    vec4 view_position = skinTransformH(blend, position, modelview_matrix);
    vec3 view_normal = normalize(mat3(modelview_matrix) *
                                 skinDirection(blend, normal));
#else
    vec4 view_position = modelview_matrix * vec4(position, 1.0);
    vec3 view_normal = normalize(mat3(modelview_matrix) * normal);
#endif

    gl_Position = projection_matrix * view_position;
    // Preserve the existing ghost placement/rim/distortion/varying code here.
}
```

Adversarial review:

- Do not call `getSkinBlend()` separately for position and normal; the upstream precision contract
  asks consumers to compute it once so the results agree bit-for-bit.
- Do not transform normals as points or add `skin_origin` to them.
- Match upstream's current convention exactly: `mat3(modelview_matrix)` applies the active
  `LLClientOuterTransform`. Do not add the outer scale again in GLSL.

Risk: shader-drift/API-contract. Confidence: high.

### Actor-ghost program creation and alpha uniform

```cpp
gActorGhostProgram.mName = "Actor Ghost Shader";
gActorGhostProgram.mShaderFiles.clear();
gActorGhostProgram.mShaderFiles.emplace_back(
    "interface/actorghostV.glsl", GL_VERTEX_SHADER);
gActorGhostProgram.mShaderFiles.emplace_back(
    "interface/actorghostF.glsl", GL_FRAGMENT_SHADER);
gActorGhostProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
success = gActorGhostProgram.createShader(LLGLSLShader::VARIANT_RIGGED);
```

At the draw sites keep explicit state on every base/rigged transition:

```cpp
static const LLStaticHashedString use_vertex_alpha("ghostUseVertexAlpha");
shader->uniform1i(use_vertex_alpha, alpha_pool_or_pbr ? 1 : 0);
```

Reset the uniform before leaving the feature draw if the same program can be reused for a draw class
with different alpha semantics.

Adversarial review:

- Testing only the rigged program misses static attachment faces, which use the base actor program.
- A fallback highlight program may not declare the uniform; an unconditional write can hide a wrong
  program choice behind a harmless-looking missing location.
- `ghostUseVertexAlpha=1` on opaque batches changes clone opacity twice if C++ already premultiplies
  the tint alpha.

Risk: shader-drift. Confidence: high.

### Previous avatar-local palette: CPU layout

Upstream current-frame layout is four origin floats followed by `count * 12` rebased palette floats.
The previous-frame cache must use the same shape. The following helper expresses the required packing:

```cpp
static void pack_rebased_palette(const std::vector<LLMatrix4a>& palette,
                                 const LLVector3& origin,
                                 std::vector<F32>& packed)
{
    const U32 count = static_cast<U32>(palette.size());
    packed.resize(4 + count * 12);

    F32* out = packed.data();
    out[0] = origin.mV[VX];
    out[1] = origin.mV[VY];
    out[2] = origin.mV[VZ];
    out[3] = 0.f;
    out += 4;

    for (U32 i = 0; i < count; ++i)
    {
        const F32* m = palette[i].mMatrix[0].getF32ptr();
        const U32 n = i * 12;
        out[n + 0]  = m[0];
        out[n + 1]  = m[1];
        out[n + 2]  = m[2];
        out[n + 3]  = m[12] - origin.mV[VX];
        out[n + 4]  = m[4];
        out[n + 5]  = m[5];
        out[n + 6]  = m[6];
        out[n + 7]  = m[13] - origin.mV[VY];
        out[n + 8]  = m[8];
        out[n + 9]  = m[9];
        out[n + 10] = m[10];
        out[n + 11] = m[14] - origin.mV[VZ];
    }
}
```

For frame `N`, preserve the fully packed data from `N-1` before overwriting the current cache. If the
previous cache is absent, upload current packed data as both current and previous so first-frame limb
velocity is zero.

The adapted upload must split both origin headers from both palette tails:

```cpp
const bool previous_valid =
    mpc.mLastFrame == gFrameCount - 1 &&
    mpc.mLastGLMp.size() == mpc.mGLMp.size();
const std::vector<F32>& previous =
    previous_valid ? mpc.mLastGLMp : mpc.mGLMp;

LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
shader->uniformMatrix3x4fv(LLShaderMgr::AVATAR_MATRIX,
                           count,
                           false,
                           reinterpret_cast<const GLfloat*>(&mpc.mGLMp[4]));
shader->uniform3fv(LLShaderMgr::SKIN_ORIGIN,
                   1,
                   reinterpret_cast<const GLfloat*>(&mpc.mGLMp[0]));

shader->uniformMatrix3x4fv(LLShaderMgr::AVATAR_LAST_MATRIX,
                           count,
                           false,
                           reinterpret_cast<const GLfloat*>(&previous[4]));
shader->uniform3fv(LLShaderMgr::LAST_SKIN_ORIGIN,
                   1,
                   reinterpret_cast<const GLfloat*>(&previous[0]));
```

Add `LAST_SKIN_ORIGIN` / `"last_skin_origin"` beside the existing previous-palette reserved entry.
Unlike hashed feature controls, these are indexed reserved uniforms used by the hot upload path.

Adversarial review:

- Do not recompute the previous origin from the avatar's current root; teleport/root motion would be
  erased from velocity.
- Do not subtract the current origin from previous joint translations.
- Cache keys must include skin identity and frame continuity. Reusing a palette across a skin swap
  creates explosive velocities.
- `packed.data()+4` is the palette start; uploading from `packed.data()` as `matrixPalette` shifts all
  joints by one vec4. The UBO/uniform upload code must bind the origin header and palette ranges to
  their intended declarations.

Risk: API-contract. Confidence: high design, medium integration.

### Previous avatar-local palette: GLSL helpers and velocity

Add a previous contract parallel to upstream `objectSkinV.glsl`:

```glsl
uniform mat3x4 lastMatrixPalette[MAX_JOINTS_PER_MESH_OBJECT];
uniform vec3 last_skin_origin;

mat3x4 getLastSkinBlend()
{
    vec4 w = fract(weight4);
    vec4 index = clamp(floor(weight4),
                       vec4(0.0),
                       vec4(MAX_JOINTS_PER_MESH_OBJECT - 1));
    w *= 1.0 / (w.x + w.y + w.z + w.w);

    mat3x4 blend = lastMatrixPalette[int(index.x)] * w.x;
    blend += lastMatrixPalette[int(index.y)] * w.y;
    blend += lastMatrixPalette[int(index.z)] * w.z;
    blend += lastMatrixPalette[int(index.w)] * w.w;
    return blend;
}

vec3 lastSkinPoint(mat3x4 blend, vec3 pos)
{
    return mat3(blend) * pos +
           vec3(blend[0].w, blend[1].w, blend[2].w);
}

vec4 lastSkinTransformH(mat3x4 blend, vec3 pos, mat4 transform)
{
    return transform * vec4(lastSkinPoint(blend, pos), 0.0) +
           transform * vec4(last_skin_origin, 1.0);
}
```

Then rewrite the rigged branch of `velocityV.glsl` and `velocityAlphaV.glsl`:

```glsl
mat3x4 current_blend = getSkinBlend();
vec4 current_view = skinTransformH(current_blend,
                                   position,
                                   modelview_matrix);

gl_Position = projection_matrix * current_view;
vary_cur_clip = projection_matrix_unjittered * current_view;

mat3x4 previous_blend = getLastSkinBlend();
vec4 previous_view = lastSkinTransformH(previous_blend,
                                        position,
                                        last_modelview_matrix);
vary_last_clip = last_projection_matrix_unjittered * previous_view;
```

This assumes `last_modelview_matrix` contains the previous camera and object/outer transform exactly
once. If upstream splits those matrices, use the actual previous combined matrix rather than
multiplying `last_object_matrix` again.

Adversarial review:

- The denominator can be zero for corrupt weights. Match upstream's current helper behavior exactly
  or introduce the same guard in both current and previous paths; asymmetric guards create motion.
- Current and previous joint counts can differ after attachment/skin replacement. Clamp to the valid
  common layout or force zero velocity for the discontinuity.
- A clone outer-scale revision between frames is motion and must be present in the previous/current
  model-view pair, not baked into the skin palettes.

Risk: API-contract/shader-drift. Confidence: medium-high.

### Outer-transform matrix composition

The adapted upstream draw path must retain the fork cache key and ordering:

```cpp
void LLRenderPass::applyModelMatrix(const LLDrawInfo& params)
{
    LLClientOuterTransform* outer = params.mOuterTransform.get();
    const U32 revision = outer ? outer->mRevision : 0;

    if (params.mModelMatrix != gGLLastMatrix ||
        outer != sLastOuterTransform ||
        revision != sLastOuterTransformRevision ||
        (outer && !params.mModelMatrix))
    {
        gGLLastMatrix = params.mModelMatrix;
        sLastOuterTransform = outer;
        sLastOuterTransformRevision = revision;

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(gGLModelView);
        if (outer && outer->mEnabled &&
            !is_approx_equal(outer->mScale, 1.f))
        {
            gGL.multMatrix(
                reinterpret_cast<GLfloat*>(outer->mCurrent.mMatrix));
        }
        if (params.mModelMatrix)
        {
            gGL.multMatrix(
                reinterpret_cast<const GLfloat*>(params.mModelMatrix->mMatrix));
        }
        gPipeline.mMatrixOpCount++;
    }
}
```

After porting, verify where upstream marks `UB_MATRICES` dirty. The final multiplied model-view must be
the matrix uploaded to the shader; updating the UBO before the outer multiplication makes bounds look
correct on CPU while the GPU remains unscaled.

Adversarial review:

- Pointer-only caching is insufficient because clone scale animates in-place; revision must be part
  of the key.
- Applying outer after the draw's local matrix changes the pivot/order (`M * Outer` instead of
  `Outer * M`). Preserve the fork's established multiplication order.
- Non-uniform scale would require inverse-transpose normal handling. The current product uses uniform
  scale; reject future non-uniform values until normal matrices are redesigned.

Risk: API-contract. Confidence: high.

### Interleaved alpha versus upstream impostors

The merged pass depth precondition must drop the removed flag:

```cpp
const bool write_depth_always =
    LLDrawPoolWater::sSkipScreenCopy ||
    getType() == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER;
```

The conservative first port can keep impostor/capture routes on upstream's legacy bake path:

```cpp
bool LLPipeline::canUseInterleavedAlpha()
{
    static LLCachedControl<bool> enabled(
        gSavedSettings, "RenderInterleavedAlpha", true);
    return enabled &&
           !sRenderingHUDs &&
           !sShadowRender &&
           !gCubeSnapshot &&
           !sImpostorRender &&
           LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD;
}
```

The `!sImpostorRender` term is a deliberate behavior change from the fork gate, not a mechanical API
translation. Treat it as a load-bearing hypothesis: it protects upstream's coverage-aware bake from
the merged iterator until the bake path is explicitly proven compatible.

Adversarial review:

- If the upstream impostor flag is set only inside a narrower region than alpha dispatch, this gate
  may still admit the merged path. Verify the call stack and flag lifetime.
- Excluding impostors can change detailed/impostor appearance ordering. The slice is not accepted
  until the transition, not just steady states, is tested; if coverage or attachment parity fails,
  adapt the merged iterator to the upstream bake instead of silently retaining the exclusion.

Risk: API-contract. Confidence: medium-high.

## Clone adversarial test matrix

| Attack case | Expected invariant | Likely failure exposed |
|---|---|---|
| Teleport clone/source between consecutive velocity frames | Discontinuity is zeroed or bounded, never full-screen NaN velocity | Previous origin recomputed from current root |
| Swap rigged attachment skin while visible | First frame of new skin has zero limb velocity | Previous palette reused under hash/count mismatch |
| Animate clone scale while camera moves | Velocity contains camera + outer-scale motion exactly once | Outer transform omitted or doubled in previous matrix |
| 150x clone at high altitude | Stable normals/vertices and planted foot | Absolute transform restored or origin added twice |
| Static alpha attachment plus rigged hair | Whole ensemble drains contiguously in detailed view | Bridge stamp only covers rigged groups |
| Same clone entering impostor range during alpha overlap | No one-frame gap/float/coverage pop | Old depth pass mixed with upstream coverage bake |
| Fallback highlight shader selected | Draw remains valid without ghost-only uniforms | Unconditional uniform writes or wrong base/rigged selection |
