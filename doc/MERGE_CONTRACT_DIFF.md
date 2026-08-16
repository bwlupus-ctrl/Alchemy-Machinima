# Upstream rendering merge: contract diff

Research snapshot: fork `develop` at `47af3df7b1a535e01bf44e9151311fe56562cc6b`, upstream
`alchemy-upstream/develop` at `af0f3bd1beb228ddd5065477211e23f227bb62f0`, merge base
`7c11d3f38bd2f28736f8ede94487c9bb9ffb1e8a`. The measured divergence is 327 fork commits and
255 upstream commits; 80 files were changed on both sides. This document is analysis only. No
merge or build was performed.

Provenance boundary: `47af3df7b1a` is the committed feature surface. `stash@{0}`
(`80769c1956479`) contains optional camera-driver, output-frame, and cookie-remap experiments and is
not a dependency of the committed port.

Tags used below:

- Risk layer: `textual`, `API-contract`, or `shader-drift`.
- Confidence: `high`, `medium`, or `low`.

## Executive decision

The upstream renderer is the new contract authority. Reusing the fork versions of `pipeline.cpp`,
`llrender`, `llviewershadermgr`, shared shaders, or impostor rendering and then trying to insert
upstream changes would retain obsolete stateful texture, mutable render-target, shader-registration,
depth, and bake assumptions. Start from upstream and re-port only the fork's feature deltas.

## Contract-change table

| Theme | Old fork contract | Pinned upstream contract | Fork consumers and required action | Risk / confidence |
|---|---|---|---|---|
| T1 texture slots | `LLTexUnit`, `gGL.getTexUnit()`, type-bearing `unbind()`, and mutable filter/address state | `ALTextureSlot`, `gGL.getTextureSlot()`, sampler chosen at bind, and parameterless `unbind()` (`6ff2cddb89c`, `bcdcda3a082`, `557ecd2eb17`; `indra/llrender/altextureslot.h:54`) | Re-port every custom bind in Prism, projector shadows, weather, froxel/projvol, dust, sidecar, and 10-bit post paths. Select a named `ALSamplers::*` value at every sample site; do not restore texture-object sampling state. | API-contract: very high / high |
| T1 samplers | Filtering, wrapping, comparison, and sRGB decode are texture-unit/texture state | `ALSampler` flags and named samplers such as `PointClamp`, `BilinearClamp`, `TrilinearWrap`, `AnisoWrap`, and `ShadowCompare` (`indra/llrender/alsamplerstate.h:61`) | Translate intent, not names. Shadow bindings must use the quality-selected compare/raw sampler; color feeds normally use point or bilinear clamp; repeating noise/LUT sources require the corresponding wrap and decode policy. | API-contract: very high / high |
| T1 render targets | Mutable storage; callers may reallocate or mutate a target's texture after allocation | Immutable attachments; `LLRenderTarget::allocate(width,height,format,depth,stencil,type,mips,depth_format)` and `resize()` rebuild storage (`f07c65fd295`; `indra/llrender/llrendertarget.h:103,109`) | Adapt `mPrismLensOutput[]`, `mPrismSpotShadow[]`, visible-diffuse sidecar, 10-bit chain, weather/froxel/projvol targets, and custom LUT/noise allocation. Never resize a bound target or a target sharing depth. Cookie targets are stash-only and excluded unless separately accepted. | API-contract: very high / high |
| T1 image allocation | `LLImageGL::setManualImage` and raw allocation in `newview` | Sized immutable allocation through `LLImageGL::allocateTexture2D` (`392129c0341`, `39c1cab7c68`) | Recreate fork-only dust, CGLUT, noise, and other raw-texture setup as resource allocation. A symbol rename is insufficient because storage lifetime and mip declarations changed. | API-contract: high / high |
| T2 probes | Irradiance cubemap `mIrradianceMaps`; raw `GLuint mUBO`; Prism uses `glBindBuffer`/`glBufferSubData` to replace one `ReflectionProbeData` record | Nine RGB SH coefficients per probe in `LLRenderTarget mSHCoeffs`; `ALUniformBuffer mUBO`; SSR controls `iterationCount` and `glossySampleCount` now live in `ReflectionProbeData` (`59601e8eba7`; `llreflectionmapmanager.h:66,108,112,225,248`) | Preserve Prism's one-probe containment, default-probe camera-space re-expression, hero/SSR suppression, and stable snapshot scale correction. Bind `mSHCoeffs` with `ALSamplers::PointClamp`; replace raw buffer calls with `mUBO.allocated()` and `mUBO.update()`. Set both staged SSR fields to zero and replay the committed `prism_auxiliary` guards around both SSR branches and hero taps in upstream SH `reflectionProbeF.glsl`; old per-bind float uploads no longer control the block. | API-contract: high / high |
| T2 probe snapshot scale | Prism corrects a stale cube-snapshot `ambscale=0`/`radscale=.5` leak before auxiliary rendering | Upstream `updateUniforms()` still emits those capture scales for irradiance snapshots (`llreflectionmapmanager.cpp:1318-1324`) before uploading at about `:1490` | The flicker hazard still exists even though irradiance storage is SH. Retain the stable `refParams` correction in the staged Prism record and restore the original record after the feed. | API-contract: high / medium |
| T3 shadows | Texture comparison state is mutated on each shadow target; legacy filtering and fixed spot bias conventions | Gather PCF, receiver-plane depth bias, PCSS; `bindShadowMaps()` selects raw `PointClamp` for quality >= 3 and `ShadowCompare` otherwise (`e8b487a8160`; `pipeline.cpp:9090`) | Remove direct `glTexParameteri`/old texture-unit compare setup from `generatePrismSpotShadows`. Prism and projvol must use the central shadow binder and the same quality contract as main-view shadows. | API-contract + shader-drift: high / high |
| T4 depth | Forward-Z projection, bias, and comparisons assumed in custom CPU/shader paths | Reverse-Z supported; `REVERSE_Z` injected at the compile choke point (`ab2d54b1b8d`; `llshadermgr.cpp:714-716`); shadow compare is GEQUAL | Use `al_perspective` and the upstream reverse-Z shadow bias mapping for Prism spot maps. Audit weather occlusion, froxel depth reconstruction, projvol, velocity, DoF, and every direct `clip.z` comparison. Keep projector-cookie UV projection forward-Z. | API-contract + shader-drift: high / high |
| T5 shader lifecycle | `LLViewerShaderMgr::mShaderList`, a fork-populated `finalizeShaderList()`, explicit custom-program registration, and `make_rigged_variant()` pairs | `LLGLSLShader::sInstances` tracks programs automatically; `createShader(VARIANT_RIGGED)` owns the rigged variant; upstream retains a small `finalizeShaderList()` assertion shell that calls `llassert(no_redundant_shaders())` (`e5d05ad11fc`, `f359c7f5471`; `llviewershadermgr.cpp:442,449,473`) | Do not reintroduce `mShaderList` or the fork finalizer body. Preserve the upstream assertion shell. Load each custom program and its GLSL in the same vertical slice. Convert actor-ghost and velocity rigged variants to `createShader(VARIANT_RIGGED)` and use `mRiggedVariant`. | API-contract: high / high |
| T5 shared state | Individual uniforms and forward-light arrays are uploaded per program | `UB_LIGHTS`, `UB_DEFERRED`, `UB_MATRICES`, `UB_ENVIRONMENT`, and `ALUniformBuffer` (`0ade53ca050`, `036c7375462`, `7750c5fce5b`, `66faf40ccaf`, `f56029be886`) | Declare correct shader features so fork programs receive engine blocks. Do not recreate removed globals. Update only truly committed feature-owned uniforms such as Prism lens, weather, or ghost controls; cookie remap is optional stash work. | shader-drift: high / high |
| T5 uniform namespace | Fork additions coexist with an older large reserved-uniform table | Unused reserved entries/globals removed; indexed `hasUniform(U32)` added (`0c94ec52d39`, `30221e9ba76`) | Reapply only live fork-specific enum/name entries after upstream cleanup. Use `hasUniform()` only for reserved uniform indices; hashed custom setters already ignore a missing location. | shader-drift: high / high |
| T6 PBR/legacy alpha | Fork shared alpha/PBR shaders predate upstream BRDF, AO, punctual-light, and legacy-alpha corrections | Corrected BRDF/integration, AO placement, punctual lighting, and alpha lighting (`12dc96aa56f`, `ed2558c`, `9517e0af79e`, `03c0860`) | Treat upstream `alphaF`, `pbralphaF`, `reflectionProbeF`, and material draw pools as authoritative. Replay only committed Prism containment hooks, sidecar outputs, mask override, and interleaved dispatch deltas. Cookie remap is a later, optional stash slice. | textual + shader-drift: high / high |
| T6 forced mask | Fork may override alpha cutoff for legacy material faces | Upstream substantially changed `LLVOVolume::registerFace`; PBR/GLTF remains a separate material contract | Reinsert the existing override after the upstream draw-info cutoff is initialized, retaining `!gltf_mat` (`llvovolume.cpp:5914-5955` in the fork). Never force legacy cutoff onto GLTF. | textual + API-contract: medium-high / high |
| T7 impostors | Fork uses an extra `sImpostorRenderAlphaDepthPass` and interleaved code references it | Upstream stamps opaque coverage before forward alpha, accumulates real alpha coverage, rebases normals, and removes the third depth-only scene render (`cda2d4fb0c1`, `0ef9c3d7f53`; `pipeline.cpp:12025,12303`) | Keep upstream `generateImpostor()` and `LLVOAvatar::renderImpostor()` intact. Do not resurrect `sImpostorRenderAlphaDepthPass`. Adapt the interleaved gate/iterator without replacing upstream coverage and material behavior. | API-contract: high / high |
| T8 rigged skinning | `getObjectSkinnedTransform()` returns an absolute matrix; fork adds `getLastObjectSkinnedTransform()` for velocity | Palette translations are avatar-local, preceded by `skin_origin`; shaders use `getSkinBlend()`, `skinDirection`, `skinPoint`, and `skinTransformH()` (`aff85c50ddb`; `objectSkinV.glsl`) | Rewrite `actorghostV`, rigged velocity shaders, and previous-palette upload. Add a matching previous origin and rebased previous palette; do not restore absolute transforms. | API-contract + shader-drift: high / high |
| T8 clone outer transform | Clone scale/rotation is applied by `LLClientOuterTransform` in the model matrix | Upstream local skinning reconstructs the origin through the active model-view matrix | The composition remains valid: the local skin term receives outer scale/rotation and `skin_origin` receives pivot translation. Ensure `applyModelMatrix()` applies the outer transform before shader matrix synchronization and the uploaded origin remains untransformed. Validate 100x/150x in-world. | API-contract: medium / high analytically |
| T9 draw paths | Fork alpha pool has attachment filters, sidecar MRT guarding, forced mask, and interleaved streams | Upstream draw pools include sampler, PBR-alpha, and impostor corrections | Transplant narrow feature deltas into upstream functions. Copying the fork's entire draw pools would regress T1/T6/T7. | textual + API-contract: high / high |

## High-risk fork call-site inventory

### Render-target and sampler consumers

- Prism retained outputs are allocated by `LLPipeline::allocatePrismLensOutput` around
  `indra/newview/pipeline.cpp:1456` and used by `LLPrismLens::renderAuxiliaryView` at
  `indra/newview/llprismlens.cpp:5447-5880`.
- Prism's target guard also captures the current target and `LLRenderTarget::sCurResX/Y` at
  `llprismlens.cpp:4818-4942`; re-test this against upstream resize/bind invariants.
- Dedicated feed spot maps use obsolete mutable comparison state at
  `pipeline.cpp:19989-19999`.
- Additional fork-only old-contract users exist in the weather rain occlusion path near
  `pipeline.cpp:17982-18001`, 3D dust near `pipeline.cpp:2648-2681` and `:14496`, and the
  froxel, projector-volumetric, sidecar, and 10-bit post chains.
- The stock old API also appears widely in the fork, but those stock sites disappear by taking
  the upstream files. Only replayed feature code should be translated.

### Reflection probes

Fork Prism state is in `pipeline.cpp:9338-9564`. It copies `ReflectionProbeData`, clamps
`refmapCount` to one, re-expresses the default probe in auxiliary-camera coordinates, normalizes
snapshot scales, uploads a temporary record, and restores it afterward. The data model survives;
only its irradiance source and UBO API change. Upstream binds the radiance cubemap plus SH target in
`pipeline.cpp:10328-10347`.

### Shadows and depth

- Upstream shadow binding is centralized at `pipeline.cpp:9090-9177`.
- Upstream spot shadow rendering uses `al_perspective` and a reverse-Z-aware bias transform near
  `pipeline.cpp:11808-11821`.
- Projector-cookie projection intentionally remains `glm::perspective` near
  `pipeline.cpp:10168-10175`; converting it would invert cookie UV/depth semantics.
- Fork `generatePrismSpotShadows` is at `pipeline.cpp:19762-20111`; it must mirror the first path,
  not the cookie path.

### Shader lifecycle and rigging

- Fork custom registration/finalization is at `llviewershadermgr.cpp:430-532`.
- Fork actor-ghost variant creation is at `llviewershadermgr.cpp:4352-4358`.
- `actorghostV.glsl:44,55`, `velocityV.glsl`, and `velocityAlphaV.glsl` call the removed absolute
  skinning helpers.
- `prismLensF/V.glsl` use feature-owned lens uniforms plus standard matrices; they do not require
  `UB_LIGHTS` and contain no direct depth comparison.

## Settings three-way result

Top-level keys were parsed independently at the merge base, fork, and pinned upstream for both
`app_settings/settings.xml` and `app_settings/settings_alchemy.xml`. There are **zero key-level
three-way collisions**. The files will conflict textually, but the sides changed disjoint keys.

Upstream removes these settings:

- `RenderCompressTextures`
- `RenderSpecularResX`
- `RenderSpecularResY`
- `RenderReflectionProbeIrradianceResolution`

Do not reintroduce them. The current fork still has consumers in feature tables, preferences,
`llappviewer.cpp`, `llviewercontrol.cpp`, `pipeline.cpp`, `llimagegl.h`, and
`llreflectionmapmanager.cpp`; those stock consumers vanish when upstream is the base. Reapply the
disjoint VCam, clone, interleaved-alpha, weather, projector, and sidecar keys.

Risk layer: textual/API-contract. Confidence: high.

## Feature-table three-way result

The Windows, Linux, and macOS feature tables also have **zero entry-level three-way collisions**.
Upstream:

- removes `RenderCompressTextures` and the `VRAMGT512` section;
- adds `AlchemyRenderUBOUpdateMode` with platform-specific values (4 on Windows/Linux, 2 on macOS);
- leaves a stale `RenderReflectionProbeIrradianceResolution` entry even though the SH conversion
  removes the setting and its code consumer.

Preserve `AlchemyRenderUBOUpdateMode`; it is renderer infrastructure, not a feature gate. Drop the
compression and VRAM section with upstream, and deliberately remove the stale irradiance-resolution
entry. None of these changes disables VCam or clones. Reapply fork-only feature-table entries after
that cleanup.

Risk layer: textual/API-contract. Confidence: high.

## Explicit redesigns and unresolved runtime questions

The following are not mechanical ports:

1. Previous-frame rigged velocity must be redesigned around rebased palettes plus a previous
   `skin_origin`.
2. Prism auxiliary probe containment must bind SH coefficients and use `ALUniformBuffer::update`.
3. Prism spot shadows must adopt central sampler selection and reverse-Z matrices.
4. Interleaved alpha must be laid over the new impostor bake without the removed third depth pass.
5. Raw custom texture allocation must become immutable resource allocation.

The code evidence is sufficient to specify these edits, but feed flicker, 100x/150x clone scaling,
impostor transitions, PCSS projector shadows, and depth-driven weather/froxel behavior remain runtime
acceptance items. Confidence for those visual outcomes is medium until a later build and in-world
test by the designated builder/user.

## Concrete contract migrations

These are implementation-shaped examples taken from the current fork and pinned upstream APIs. They
show the minimum transformation expected in feature patches; surrounding error handling and feature
gates still have to be retained.

### Stateful texture unit to sampled texture slot

Current fork pattern:

```cpp
gGL.getTexUnit(channel)->bind(texture);
gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
// draw
gGL.getTexUnit(channel)->unbind(LLTexUnit::TT_TEXTURE);
```

Pinned-upstream form:

```cpp
ALTextureSlot* slot = gGL.getTextureSlot(channel);
slot->bindSampled(texture, ALSamplers::BilinearClamp);
// draw
slot->unbind();
```

For a render-target color attachment, prefer the target helper:

```cpp
target.bindTexture(0, channel, ALSamplers::BilinearClamp);
```

Adversarial review:

- A mechanical `getTexUnit` -> `getTextureSlot` rename is wrong if the old filter/address calls are
  simply deleted; the target will inherit whichever sampler was left on the slot.
- `SRGBDecode` is opt-in. Adding it to an already shader-linearized source double-decodes; omitting it
  from an intended hardware-linearized source changes brightness.
- A multisample texture must not be paired with a sampler object.

Risk: API-contract. Confidence: high.

### Mutable Prism target allocation to immutable storage

Current fork:

```cpp
return output.allocate(PRISM_OUTPUT_CAPACITY,
                       PRISM_OUTPUT_CAPACITY,
                       GL_RGBA16F);
```

Adapted allocation:

```cpp
return output.allocate(PRISM_OUTPUT_CAPACITY,
                       PRISM_OUTPUT_CAPACITY,
                       GL_RGBA16F,
                       false,                         // no retained depth attachment
                       false,                         // no stencil
                       ALTextureSlot::TT_TEXTURE,
                       LLRenderTarget::MIPS_NONE);
```

If a future feed genuinely samples depth, define that consumer and its depth-format contract before
allocating it; do not assume the main framebuffer's format and do not add depth defensively. The
committed retained feed is color-only. Its fixed 1024x1024 lifetime policy is compatible with
immutable storage and avoids destructive resizes of retained frames.

Adversarial review:

- Calling `release()` or `resize()` while the target is in the RT stack asserts.
- Sharing depth and later resizing the retained target is unsupported.
- Passing an unsized GL format or assuming old allocation set filter/wrap state is invalid.

Risk: API-contract. Confidence: high.

### Raw probe UBO mutation to `ALUniformBuffer`

Current fork:

```cpp
glBindBuffer(GL_UNIFORM_BUFFER, mReflectionMapManager.mUBO);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(prism_probe_data), &prism_probe_data);
```

Adapted transaction:

```cpp
if (mReflectionMapManager.mUBO.allocated())
{
    mReflectionMapManager.mUBO.update(&prism_probe_data,
                                      sizeof(prism_probe_data));
    mPrismProbeUBOStaged = true;
}

// In endPrismAuxiliaryState(), before the main-view bind:
if (mPrismProbeUBOStaged && mPrismSavedProbeDataValid)
{
    mReflectionMapManager.mUBO.update(&mPrismSavedProbeData,
                                      sizeof(mPrismSavedProbeData));
}
```

The shader binder must still call `mReflectionMapManager.setUniforms()` so the manager attaches its
buffer to `LLGLSLShader::UB_REFLECTION_PROBES`.

Adversarial review:

- Updating only the first probe element is wrong: `update()` replaces the entire buffer contents.
  Upload the complete `ReflectionProbeData` struct both ways.
- An early return between staging and restoration contaminates the main view. The actual port must
  keep the existing scoped begin/end guard or introduce RAII.
- `mProbeData` can change between frames; saving it once at feature initialization is stale. Capture
  it at the start of every auxiliary transaction.

Risk: API-contract. Confidence: high.

### Shadow target binding without mutable comparison state

Do not port this fork code:

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
```

Use the same expression that governs the compiled GLSL sampler type:

```cpp
static LLCachedControl<U32> shadow_quality(
    gSavedSettings, "AlchemyRenderShadowFilterQuality", 1);
const bool pcss = llclamp(shadow_quality(), 0u, 3u) >= 3;
const U32 sampler = gGL.getSampler(
    pcss ? ALSamplers::PointClamp : ALSamplers::ShadowCompare);

S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + 4 + slot);
if (channel > -1)
{
    gGL.getTextureSlot(channel)->bind(getSpotShadowTarget(slot), true, sampler);
}
```

Prefer calling/extending upstream `bindShadowMaps()` instead of duplicating this code. The snippet
documents the invariant for a dedicated path.

Adversarial review:

- Adding `sPrismLensRender`, cube-capture state, or an adaptive-quality override to the `pcss`
  expression can make the runtime sampler disagree with the shader's compiled type.
- Forward-Z `GL_LEQUAL` is wrong under upstream reverse-Z; the named compare sampler owns GEQUAL.
- Leaving a compare sampler on a unit later used for a color texture is undefined. Preserve
  upstream's compare-unit cleanup mask.

Risk: API-contract/shader-drift. Confidence: high.

### Manual rigged pair to owned shader variant

Current fork:

```cpp
bool ghost_ok = make_rigged_variant(gActorGhostProgram,
                                    gSkinnedActorGhostProgram);
ghost_ok = ghost_ok && gActorGhostProgram.createShader();
```

Adapted upstream lifecycle:

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

Consumers use `gActorGhostProgram` for static geometry and
`gActorGhostProgram.mRiggedVariant` for rigged geometry.

Adversarial review:

- Calling base `createShader()` again after `createShader(VARIANT_RIGGED)` can replace or duplicate
  lifecycle state depending on upstream implementation. Follow upstream's one-call pattern.
- Keeping the cpp-local `gSkinnedActorGhostProgram` creates a second live program name and defeats
  automatic instance validation. Remove its definition and all references; there is no header
  declaration to delete.

Risk: API-contract. Confidence: high.

### Absolute object skinning to avatar-local blend

Current fork vertex pattern:

```glsl
mat4 skin = getObjectSkinnedTransform();
vec4 view_pos = modelview_matrix * skin * vec4(position, 1.0);
```

Adapted upstream pattern:

```glsl
mat3x4 skin = getSkinBlend();
vec4 view_pos = skinTransformH(skin, position, modelview_matrix);
vec3 view_normal = normalize(mat3(modelview_matrix) * skinDirection(skin, normal));
```

Adversarial review:

- Computing a normal as two transformed positions reintroduces the altitude precision bug.
- Adding `skin_origin` inside `skinPoint()` and again in `skinTransformH()` double-translates.
- Applying `LLClientOuterTransform` to the palette on CPU and in model-view double-scales clones.

Risk: API-contract/shader-drift. Confidence: high.

## Adversarial merge review matrix

| Proposed shortcut | Why it appears safe | Failure it hides | Required rejection test |
|---|---|---|---|
| Copy fork `pipeline.cpp` and insert upstream hunks | Fewer custom lines to move | Leaves old target, sampler, probe, depth, and impostor assumptions in unconflicted code | Search resulting custom regions for every retired symbol and compare upstream choke-point bodies |
| Resolve shared shader conflicts in favor of fork | Preserves local visual features | Reverts corrected PBR, SH probes, PCSS, reverse-Z, and UBO interfaces | Shared-file base SHA/diff must be upstream-led with only named feature hunks replayed |
| Keep old impostor depth flag for interleaving | Existing interleaved code references it | Recreates a pass upstream removed and corrupts the new coverage order | Zero references to `sImpostorRenderAlphaDepthPass` after port |
| Use default target sampler everywhere | Fewer sampler decisions | Attachment 0/data/depth defaults differ and sRGB intent becomes accidental | Every custom sample site documents filter, address, compare, and decode intent |
| Upload only changed probe entry | Lower UBO traffic | `ALUniformBuffer::update` replaces storage and leaves the rest undefined/missing | Full-struct byte size in stage and restore paths |
| Retain old previous palette | Less velocity work | Current and previous frames use different coordinate spaces, producing huge motion vectors | Current/previous palettes both contain origin header and rebased translations |
