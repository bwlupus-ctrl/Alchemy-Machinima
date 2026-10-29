# VCam / Prism adaptation specification

Base assumptions: committed fork `47af3df7b1a`, upstream `af0f3bd1beb`, merge base
`7c11d3f38bd`. The optional working-tree experiment is `stash@{0}` / `80769c1956479`; it is not part
of the committed VCam feature and is excluded from the required merge unless separately accepted.
This is a re-port specification, not an instruction to merge the analyzed branches directly.

Classification:

- `unchanged`: upstream does not alter the depended-on contract.
- `mechanical`: a narrow API spelling/signature update.
- `semantic`: behavior or lifetime must be redesigned against upstream.
- `validate`: code direction is known, but visual/runtime acceptance is required.

## Direct answers

### 1. Retained feed targets and immutable storage

`renderAuxiliaryView()` does survive, but its allocation plumbing does not survive unchanged.
Upstream keeps `LLRenderTarget`, while replacing mutable attachments with sized immutable storage
(`f07c65fd295`). Re-port `mPrismLensOutput[]` as ordinary upstream targets allocated through:

```cpp
target.allocate(width, height, color_format, false, false,
                ALTextureSlot::TT_TEXTURE,
                LLRenderTarget::MIPS_NONE);
```

Use the actual retained color format. The retained feed is sampled as color and has no depth
consumer, so do not allocate a depth attachment merely because the main renderer has one.
When size or format changes, release/reallocate outside a bind, or call upstream `resize()` only when
the target is not bound and does not share depth. Keep the existing retained-slot registry and
prepare-all / choose-one / mark-produced scheduling.

Fork anchors: `pipeline.cpp:1456` (`allocatePrismLensOutput`), `:1507`
(`bindPrismLensTarget`), `llprismlens.cpp:5447-5880` (`renderAuxiliaryView`). Upstream model:
`llrendertarget.h:103,109`, `pipeline.cpp:907`.

Risk: API-contract, very high. Confidence: high.

### 2. Auxiliary reflection containment after SH projection

Yes, there is still state to contain. `ReflectionProbeData` remains, and the fork can still stage a
single default-probe record for the auxiliary camera. The irradiance input is now the
`mSHCoeffs` target instead of `mIrradianceMaps`, and the upload object is `ALUniformBuffer` instead
of a raw GL buffer (`59601e8eba7`; `llreflectionmapmanager.h:66,225,248`).

Retain all of the fork behavior in `pipeline.cpp:9408-9466`:

1. Copy the current record array.
2. Set `refmapCount=1` and hero count to zero.
3. Re-express the default probe sphere in the auxiliary camera space.
4. Correct capture-only scale values to stable display values.
5. Set staged `iterationCount=0` and `glossySampleCount=0`, suppress hero probes, and retain the
   shader-side `prism_auxiliary` guards around scene SSR and hero sampling.
6. Restore the saved record after auxiliary rendering.

Replace raw buffer binding/upload with `mUBO.allocated()` and `mUBO.update(data, size)`. In the probe
binder, bind the radiance map plus `mSHCoeffs` using `ALSamplers::PointClamp`, matching upstream
`pipeline.cpp:10328-10347`. Upstream still creates `ambscale=0` and `radscale=.5` during irradiance
snapshot processing around `llreflectionmapmanager.cpp:1318-1324`; the known flicker containment
therefore remains relevant.

Risk: API-contract, high. Confidence: high for the data path; medium for the visual flicker result.

### 3. Spot-shadow PCF/PCSS and reverse-Z

The committed fork does not contain cookie remapping. `proj_cookie_region`, `proj_cookie_orient`,
`projCookieUv()`, and the camera-driver/output-frame additions exist only in the optional WIP stash.
Do not make them a dependency of the committed VCam re-port. If that experiment is accepted later,
port it as a separate vertical slice using the exact WIP contract recorded below.

`mPrismSpotShadow[]` needs a semantic port. Upstream's `bindShadowMaps()` chooses raw depth with
`ALSamplers::PointClamp` for PCSS quality and comparison sampling with
`ALSamplers::ShadowCompare` for lower tiers (`e8b487a8160`; `pipeline.cpp:9090-9177`). Dedicated feed
maps must follow that same selection. Remove the fork's mutable comparison-state setup at
`pipeline.cpp:19989-19999`.

Feed shadow rendering must use upstream `al_perspective`, the exact depth-format policy from
`allocateShadowBuffer()`, and the reverse-Z-aware bias transform modeled at upstream
`pipeline.cpp:11808-11821`. Projector texture projection remains a separate forward
`glm::perspective` product, as upstream intentionally does around `pipeline.cpp:10168-10175`.

Risk: API-contract + shader-drift, high. Confidence: high.

### 4. Prism lens shader interface

`prismLensF.glsl` and `prismLensV.glsl` use their own lens controls and standard matrix uniforms.
They do not consume the removed forward-light arrays, so they do not need `UB_LIGHTS`. They do not
perform depth comparisons, so no explicit reverse-Z branch is required. They must be created through
the upstream shader lifecycle so the standard matrix block is available; preserve their custom
uniform-name entries and C++ uploads.

Risk: shader-drift, medium. Confidence: high.

## Per-file edit map

### `indra/newview/llprismlens.cpp`

Status: mostly `unchanged`, with `mechanical` sampler/target calls and `validate` around state guards.

- Keep the registry and scheduler at `:3068` (`chooseRenderSlot`) and `:3161` (`markProduced`).
- Keep `renderAuxiliaryView()` at `:5447`: prepare all logical feeds, select one slot, establish the
  auxiliary camera, render, copy/resolve to the retained output, then mark it produced.
- Translate the remaining `gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE)` around `:5210` to
  `gGL.getTextureSlot(0)->unbind()`. This is only an unbind; no sampler belongs on the call.
- Adapt the existing `ScopedPrismRenderState` around `:4796` and its use around `:5523`; do not add a
  second competing guard. Keep its current-target and viewport restoration, but audit it
  against immutable-target assertions. No allocation/resize is allowed while a guarded target is
  bound.
- Keep the ordering at `:4960`: activate the auxiliary probe record only after the auxiliary camera
  matrices exist.
- Keep feed-specific spot shadow generation at `:5815`, but call the re-ported implementation.
- Resolve/copy retained outputs at `:5840-5880` using upstream target APIs and named sampling.
- Keep lens display/debug consumers at `:5903`, `:6160`, and `:6356`; pass a clamp sampler when
  sampling the retained feed.

### `indra/newview/llprismlens.h`

Status: `unchanged` data model.

- Preserve capture IDs, slot ownership, retained-output semantics, and the public
  `renderAuxiliaryView()` declaration at `:502`.
- Do not embed texture/sampler objects in lens state; those remain pipeline-owned render resources.

### `indra/newview/llcinematiccamera.cpp/.h`

Status: committed surface is `unchanged` with `validate` for matrix timing; stash-only output-driving
APIs are excluded.

- Preserve committed APIs such as `updateCamera`, `applyFrameLens`, `resolveAnchor`,
  `captureCurrentSwitcherView`, and the `pattern*` helpers.
- Confirm the auxiliary camera is written before probe sphere re-expression, shadow culling, and
  render-target sizing.
- Do not translate projector texture projection to reverse-Z; only the feed's render/shadow camera
  uses the upstream depth convention.

### `indra/newview/aldirectorswitcher.*`

Status: `unchanged`.

- Upstream has zero churn in these files. Reapply the committed switcher after pipeline/camera
  contracts are stable.
- Preserve switcher-to-VCam ownership and follow/orbit/lock-on behavior. They should not be used to
  solve renderer state issues.

`alprismcamdriver.*` is untracked stash content (`stash@{0}^3`), not committed source. It belongs only
to the optional post-acceptance WIP slice.

### `indra/newview/pipeline.h`

Status: `semantic` declarations and ownership.

- Reinsert `mPrismLensOutput[LLPrismLens::MAX_CAPTURES]` near the upstream post/deferred target pack,
  preserving pipeline ownership (`fork :1012`).
- Reinsert `mPrismSpotShadow[MAX_SPOT_SHADOWS]` next to the upstream main spot shadow targets
  (`fork :1038-1041`).
- Reinsert `bindPrismLensTarget`, `beginPrismAuxiliaryState`,
  `activatePrismAuxiliaryProbeState`, `endPrismAuxiliaryState`, and
  `generatePrismSpotShadows` declarations (`fork :145,509,553-557`).
- Any saved UBO member must store `ReflectionProbeData`, not GL binding IDs. Do not expose a raw
  buffer handle.

### `indra/newview/pipeline.cpp`

Status: `semantic`; reapply grouped Prism blocks onto upstream.

- Resource lifecycle: port `allocatePrismLensOutput` (`fork :1456`) and release hooks (`:1524`) to
  immutable allocation. Allocate only the selected size/format and avoid sharing a depth target that
  later resizes.
- Target routing: port `bindPrismLensTarget` (`:1507`) after upstream target routing is in place.
- Auxiliary state: reapply `beginPrismAuxiliaryState` (`:9338`), the SH/UBO-adapted
  `activatePrismAuxiliaryProbeState` (`:9408`), and `endPrismAuxiliaryState` (`:9501`) as one patch so
  every captured field has a restore.
- Probe binding: start with upstream `bindReflectionProbes` (`upstream :10328`), then add the
  `sPrismLensRender` clamp/suppression. Keep upstream SH, hero, SSR, and sampler behavior for normal
  rendering.
- Projector setup: keep the committed projector path and upstream forward projection matrix. Cookie
  feed selection/remapping is stash-only and excluded from this slice.
- Feed spot shadows: reapply `generatePrismSpotShadows` (`fork :19762`) using the upstream shadow
  renderer's projection, bias, depth format, and sampler policy. Adapt the existing fork
  `getSpotShadowTarget()` selector (`fork :18713`) to select the dedicated array while the Prism flag
  is set; do not introduce a parallel selector.
- Composite/output: reapply the fork block around `:16883-17178`, retaining upstream postprocess
  sampling and named samplers.
- Keep main-view state byte-identical by restoring the probe record, selected spot lights, matrices,
  viewport, target, temporal state, and render flags before main `stateSort`.

### `indra/newview/llreflectionmapmanager.h/.cpp`

Status: upstream-owned plus a narrow `semantic` friend access.

- Do not bring back `mIrradianceMaps` or `GLuint mUBO`.
- The existing `friend class LLPipeline` access is sufficient for Prism staging.
- Use `ALUniformBuffer::update` to stage and restore the exact `ReflectionProbeData` array size.
- Do not bypass normal upstream `updateUniforms()` for main rendering.

### `indra/newview/llviewershadermgr.cpp/.h`

Status: `mechanical` lifecycle plus `shader-drift` audit.

- Reinsert Prism program declarations and load functions at the equivalent of fork `:1378-1384`.
- Do not add them to `mShaderList`; upstream `LLGLSLShader::sInstances` handles lifecycle and
  environment updates.
- Preserve only live committed Prism uniform enums and strings after upstream's reserved-uniform
  cleanup. Cookie uniforms are optional stash work.
- Declare standard matrix/environment features actually used by each program; do not request
  `UB_LIGHTS` for `prismLensF/V`.

### Shared and custom shaders

Status: `semantic` for shared files, `mechanical` for lens shaders.

- `prismLensF/V.glsl`: retain source, update only standard include/interface syntax required by
  upstream.
- `reflectionProbeF.glsl`: start with upstream SH code, then replay the committed
  `prism_auxiliary` guards around both scene-SSR branches and both hero-probe sampling sites.
- `spotLightF.glsl` and `shadowUtil.glsl`: keep upstream PCF/PCSS/reverse-Z implementation; never
  copy fork versions wholesale.
- Projector-volumetric shaders: use shared upstream shadow utility and the same shadow feature flag;
  audit any local raw depth comparison separately.

### Settings, UI, and feature tables

Status: `mechanical` three-way insertion.

- Reapply VCam/Prism settings because there are no key-level collisions.
- Preserve upstream `AlchemyRenderUBOUpdateMode` and removed legacy renderer keys.
- Do not add a feature-table gate that turns VCam off merely because upstream changed renderer
  quality levels; the VCam renderer should inherit the selected quality and use its adaptive-FPS
  policy independently.

## Required invariants during implementation

1. The feed uses dedicated retained outputs and dedicated spot maps; it never overwrites main-view
   targets.
2. Every auxiliary state mutation has a symmetric restore before main-view cull/sort/render.
3. Probe containment is one default probe in auxiliary camera space, with stable scales and no
   inherited hero/SSR state.
4. Prism spot maps use exactly the main renderer's active PCF/PCSS sampler contract.
5. Projector texture projection remains forward; shadow depth follows upstream reverse-Z.
6. No mutable texture parameter calls remain in re-ported VCam code.

## Acceptance checklist (later build/test phase)

- Monitor feed renders at every retained-output size and after resize.
- Adaptive FPS selects and updates only the expected slot.
- Three-lens magnifier samples the intended retained textures without edge wrap or sRGB drift.
- Projector texture projection and shadows align in the feed at all shadow quality tiers, including
  PCSS. Cookie-atlas remapping is tested only if the optional stash slice is accepted.
- Director switcher drives the VCam; follow/orbit/lock-on remain stable.
- PBR objects show no auxiliary-probe flicker and match the main view within intended camera/exposure
  differences.
- Main-view probes, shadows, and temporal state are unchanged after a feed frame.

Overall VCam port confidence: high for the edit plan, medium until visual acceptance.

## Concrete VCam port code

The following blocks are intended to be transplanted into the upstream-led implementation. They are
more precise than pseudocode, but names of newly added fork members must match the final header.

### Retained output allocation

The fork already uses a fixed-capacity retained texture, which is the safest policy under immutable
storage. Port the function as:

```cpp
bool LLPipeline::allocatePrismLensOutput(U32 slot, U32 width, U32 height)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    constexpr U32 PRISM_OUTPUT_CAPACITY = 1024;
    if (slot >= LLPrismLens::MAX_CAPTURES || width == 0 || height == 0 ||
        width > PRISM_OUTPUT_CAPACITY || height > PRISM_OUTPUT_CAPACITY)
    {
        return false;
    }

    LLRenderTarget& output = mPrismLensOutput[slot];
    if (output.isComplete())
    {
        return output.getWidth() == PRISM_OUTPUT_CAPACITY &&
               output.getHeight() == PRISM_OUTPUT_CAPACITY;
    }

    return output.allocate(PRISM_OUTPUT_CAPACITY,
                           PRISM_OUTPUT_CAPACITY,
                           GL_RGBA16F,
                           false,
                           false,
                           ALTextureSlot::TT_TEXTURE,
                           LLRenderTarget::MIPS_NONE) &&
           output.isComplete();
}
```

The logical source rectangle and UV scale still come from the produced width/height; never expose the
unused part of the 1024 square as valid feed pixels.

Adversarial review:

- Do not call `resize(width,height)` on every adaptive-FPS size change: it destroys retained storage
  and can invalidate sibling displays sampling the last produced frame.
- Do not allocate a depth attachment unless a consumer actually samples/uses it; it doubles lifetime
  and complicates resize ownership.
- If device/context recreation leaves a complete target with the wrong format, the width-only test is
  insufficient. The resource-recreation path must release all Prism outputs as one lifecycle event.

Risk: API-contract. Confidence: high.

### SH-aware auxiliary probe staging

Replace the raw UBO portion of `activatePrismAuxiliaryProbeState()` while keeping the existing record
construction and stable-scale correction:

```cpp
void LLPipeline::activatePrismAuxiliaryProbeState()
{
    if (!mPrismAuxiliaryStateActive || !mPrismSavedProbeDataValid ||
        !mReflectionMapManager.mUBO.allocated())
    {
        return;
    }

    LLReflectionMapManager::ReflectionProbeData prism_probe_data =
        mPrismSavedProbeData;
    prism_probe_data.refmapCount = llmin(prism_probe_data.refmapCount, 1);
    prism_probe_data.heroProbeCount = 0;
    // Upstream moved these SSR controls into ReflectionProbeData. Zero both in
    // the staged auxiliary record; the fork's old per-bind uniform1f calls are
    // no-ops against the uniform block.
    prism_probe_data.iterationCount = 0.f;
    prism_probe_data.glossySampleCount = 0.f;

    if (LLReflectionMap* probe = mReflectionMapManager.mDefaultProbe)
    {
        LLMatrix4a modelview;
        LLVector4a camera_origin;
        modelview.loadu(gGLModelView);
        modelview.affineTransform(probe->mOrigin, camera_origin);

        prism_probe_data.refSphere[0].set(camera_origin.getF32ptr());
        prism_probe_data.refSphere[0].mV[3] = probe->mRadius;
        prism_probe_data.refParams[0].mV[3] =
            camera_origin.getF32ptr()[2] - probe->mRadius;

        static LLCachedControl<bool> auto_adjust(
            gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        const F32 minimum_ambiance = LLEnvironment::instance()
            .getCurrentSky()->getReflectionProbeAmbiance(auto_adjust);
        const F32 stable_scale = llmax(0.f, mReflectionMapManager.mResetFade);
        prism_probe_data.refParams[0].mV[0] =
            llmax(minimum_ambiance, probe->getAmbiance()) * stable_scale;
        prism_probe_data.refParams[0].mV[1] = stable_scale;
    }

    mReflectionMapManager.mUBO.update(&prism_probe_data,
                                      sizeof(prism_probe_data));
    mPrismProbeUBOStaged = true;
}
```

Restore in `endPrismAuxiliaryState()`:

```cpp
if (mPrismProbeUBOStaged && mPrismSavedProbeDataValid &&
    mReflectionMapManager.mUBO.allocated())
{
    mReflectionMapManager.mUBO.update(&mPrismSavedProbeData,
                                      sizeof(mPrismSavedProbeData));
}
mPrismProbeUBOStaged = false;
```

Adversarial review:

- `mPrismSavedProbeData` must be captured after the current frame's main probe selection is known,
  not cached across frames.
- Every error/exception/early return after staging must flow through `endPrismAuxiliaryState()`.
- Two simultaneous auxiliary captures cannot both overwrite the manager UBO. The registry currently
  serializes one produced slot per render; keep that invariant or replace the temporary overwrite
  with a separate per-capture UBO design.
- `update()` may orphan/stream the buffer. Profiling should confirm two extra full-struct uploads per
  produced feed do not create UBO-ring pressure.
- Do not retain the fork's old `uniform1f(SSR_ITERATIONS, 0)` or glossy-sample upload as the
  containment mechanism. Upstream reads these values from `ReflectionProbeData`.

Risk: API-contract. Confidence: high correctness, medium performance.

### Probe binder delta on top of upstream

Start with upstream `bindReflectionProbes()` and make the Prism choices explicit:

```cpp
void LLPipeline::bindReflectionProbes(LLGLSLShader& shader)
{
    static const LLStaticHashedString prism_auxiliary("prism_auxiliary");
    const bool auxiliary = sPrismLensRender;
    shader.uniform1i(prism_auxiliary, auxiliary ? 1 : 0);

    if (!sReflectionProbesEnabled)
    {
        return;
    }

    bool bound = false;
    S32 channel = shader.enableTexture(LLShaderMgr::REFLECTION_PROBES);
    if (channel > -1 && mReflectionMapManager.mTexture.notNull())
    {
        mReflectionMapManager.mTexture->bind(channel);
        bound = true;
    }

    channel = shader.enableTexture(LLShaderMgr::SH_COEFFS);
    if (channel > -1 && mReflectionMapManager.mSHCoeffs.isComplete())
    {
        mReflectionMapManager.mSHCoeffs.bindTexture(
            0, channel, ALSamplers::PointClamp);
        bound = true;
    }

    if (RenderMirrors && !auxiliary)
    {
        channel = shader.enableTexture(LLShaderMgr::HERO_PROBE);
        if (channel > -1 && mHeroProbeManager.mTexture.notNull())
        {
            mHeroProbeManager.mTexture->bind(channel);
            bound = true;
        }
    }

    if (bound && (!auxiliary ||
        (mPrismSavedProbeDataValid && mReflectionMapManager.mUBO.allocated())))
    {
        mReflectionMapManager.setUniforms();
        setEnvMat(shader);
    }

    // Retain the upstream scene-map/SSR block, but execute it only for !auxiliary.
}
```

Do not omit the rest of upstream's binder. Its scene map, depth, SSR uniforms, noise phase, and compare
unit cleanup must be retained with the existing Prism `!auxiliary` gates.

Adversarial review:

- Binding radiance without SH can produce specular-only probes. Decide whether to degrade to no probe
  lighting when either resource is missing instead of setting `bound` when only one exists.
- `prism_auxiliary` may be optimized out of some variants. The hashed `uniform1i` setter safely
  ignores a missing location; do not call indexed-only `hasUniform(U32)` with a hashed name.
- Never call `updateUniforms()` from the source-camera state as a fallback inside the auxiliary pass.

Risk: API-contract/shader-drift. Confidence: medium-high.

### Required SH reflection-shader guards

Zeroing the staged SSR fields is necessary but not sufficient: the committed fork also prevents the
trace call and hero-probe taps from running during auxiliary rendering. Start from upstream's SH
`reflectionProbeF.glsl`, add `uniform int prism_auxiliary;` beside `cube_snapshot`, and replay these
four guards without replacing the upstream SSR implementation:

```glsl
#if defined(SSR)
    if (prism_auxiliary == 0 && cube_snapshot != 1 && glossiness >= 0.9)
    {
        // Keep the complete upstream PBR SSR body here.
    }
#endif

    if (prism_auxiliary == 0)
    {
        tapHeroProbe(glossenv, pos, norm, glossiness);
    }
```

The legacy path receives the same containment:

```glsl
#if defined(SSR)
    if (prism_auxiliary == 0 && cube_snapshot != 1)
    {
        // Keep the complete upstream legacy SSR body here.
    }
#endif

    if (prism_auxiliary == 0)
    {
        tapHeroProbe(glossenv, pos, norm, glossiness);
        tapHeroProbe(legacyenv, pos, norm, 1.0);
    }
```

This is defense in depth: the staged block prevents SSR work from being requested, while the shader
guard preserves the committed fork's explicit auxiliary-pass exclusion and blocks hero taps, which
do not depend on `heroProbeCount` alone.

### Dedicated spot-map allocation and reverse-Z matrix

Replace fork mutable setup with the same depth format as upstream shadows:

```cpp
static LLCachedControl<bool> depth_32f(
    gSavedSettings, "AlchemyRenderShadowDepth32F", false);
const LLRenderTarget::eDepthFormat depth_format =
    (depth_32f || LLRender::sReverseZ)
        ? LLRenderTarget::DEPTH_FMT_32F
        : LLRenderTarget::DEPTH_FMT_24;

if (mPrismSpotShadow[i].getWidth() != mSpotShadow[i].getWidth() ||
    mPrismSpotShadow[i].getHeight() != mSpotShadow[i].getHeight())
{
    mPrismSpotShadow[i].release();
    if (!mPrismSpotShadow[i].allocate(mSpotShadow[i].getWidth(),
                                      mSpotShadow[i].getHeight(),
                                      0,
                                      true,
                                      false,
                                      ALTextureSlot::TT_TEXTURE,
                                      LLRenderTarget::MIPS_NONE,
                                      depth_format))
    {
        mShadowSpotLight[i] = nullptr;
        continue;
    }
}
```

Port the shadow projection exactly from upstream:

```cpp
glm::mat4 projection = al_perspective(fovy, aspect, near_clip, far_clip);
glm::mat4 bias(0.5f, 0.0f, 0.0f, 0.0f,
               0.0f, 0.5f, 0.0f, 0.0f,
               0.0f, 0.0f, 0.5f, 0.0f,
               0.5f, 0.5f, 0.5f, 1.0f);
if (LLRender::sReverseZ)
{
    bias[2][2] = 1.0f;
    bias[3][2] = 0.0f;
}
mSunShadowMatrix[i + 4] = bias * projection * view * inv_view_aux;
```

Sampling remains centralized in `bindShadowMaps()`; the allocation function sets no comparison,
filter, or wrap state.

Adversarial review:

- `mSpotShadow[i]` can be incomplete during quality changes. Do not treat zero reference dimensions
  as a request to release/reallocate repeatedly; mark the feed slot unshadowed for that frame.
- Do not use `mainDepthFormat()` blindly if upstream shadow quality specifically forces 32F under
  reverse-Z; mirror `allocateShadowBuffer()`.
- The cookie matrix must not reuse `al_perspective`/reverse-Z bias. Shadow and cookie matrices are two
  separate products even though they share a light.

Risk: API-contract. Confidence: high.

### Feed sampling

Every display/composite consumer should name its sampling intent:

```cpp
S32 channel = shader.enableTexture(LLShaderMgr::DIFFUSE_MAP);
if (channel > -1)
{
    mPrismLensOutput[capture_slot].bindTexture(
        0, channel, ALSamplers::BilinearClamp);
}
```

Use `PointClamp` instead if the current fork requires texel-exact magnifier sampling. Make that a
product decision per consumer rather than accepting `TargetDefault` accidentally.

## Optional post-acceptance WIP cookie slice

This section is provenance, not a required committed-fork port. If `stash@{0}` is accepted after the
committed feature passes, its cookie UV ABI must be preserved exactly: both uniforms are `vec4`,
`.xy` is scale, `.zw` is offset, orientation is applied before atlas-region remapping, and the
identity value is `(1, 1, 0, 0)`.

```glsl
uniform vec4 proj_cookie_orient;
uniform vec4 proj_cookie_region;

vec2 projCookieUv(vec2 tc)
{
    vec2 cookie_tc = tc * proj_cookie_orient.xy + proj_cookie_orient.zw;
    cookie_tc = cookie_tc * proj_cookie_region.xy + proj_cookie_region.zw;
    return cookie_tc;
}
```

The matching C++ upload is two `uniform4fv` calls populated from
`orient_scale/orient_offset` and `region_scale/region_offset`. Land that upload, its shader
declarations, its call sites, and `alprismcamdriver.*` together. Do not partially import the stash or
reinterpret either vector as a rotation/flip enum.

## VCam adversarial acceptance cases

| Attack case | Expected invariant | Instrumentation |
|---|---|---|
| Feed allocation fails for one slot | Previous valid slots remain sampleable; failed slot is not published | Log slot/generation and inspect registry validity |
| Shadow quality changes PCF <-> PCSS mid-session | Shader variant and sampler type switch together | GL debug output plus shader/sampler tier log |
| Probe cube snapshot immediately precedes feed | Feed ambiance/radiance scale remains stable | Capture staged `refParams[0]` and compare consecutive frames |
| Auxiliary render exits early after cull | Main UBO, lights, matrices, viewport, target, and Poisson phase restore | Hash/snapshot all saved fields before and after |
| Two displays request different sizes from one slot | Fixed texture persists; logical UV regions differ correctly | Debug border outside produced rectangle |
| VCam disabled | Main framebuffer and renderer-state hashes match a build without the feature patch | Frame capture and state counters |
