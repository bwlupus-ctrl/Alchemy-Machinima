# Shader-interface drift audit

Snapshot: fork `47af3df7b1a`, pinned upstream `af0f3bd1beb`, merge base `7c11d3f38bd`.
This audit covers fork-added shaders and shared shaders modified on both sides. It is a source-level
contract audit; no shader compilation or viewer build was performed.

Provenance boundary: the required shader surface is committed at `47af3df7b1a`. Cookie remapping and
its camera-driver/output-frame CPU writers exist only in `stash@{0}` (`80769c1956479`) and are an
optional post-acceptance slice.

## Upstream interface changes that govern every port

1. `LLGLSLShader::sInstances` owns live-program tracking; `mShaderList` and the fork-populated
   finalizer body must not return. Preserve upstream's small `finalizeShaderList()` shell and its
   `llassert(no_redundant_shaders())` check (`e5d05ad11fc`; upstream
   `llviewershadermgr.cpp:442-449,473`).
2. Rigged variants are created with `createShader(LLGLSLShader::VARIANT_RIGGED)` and reached through
   `mRiggedVariant` (`f359c7f5471`).
3. Shared state moved to `UB_LIGHTS`, `UB_DEFERRED`, `UB_MATRICES`, and `UB_ENVIRONMENT`
   (`0ade53ca050`, `036c7375462`, `7750c5fce5b`, `66faf40ccaf`). Custom programs receive them through
   feature declarations, not ad-hoc registration.
4. `REVERSE_Z` is injected by the compile choke point (`llshadermgr.cpp:714-716`), so custom shaders
   created normally inherit it. Do not add it to `sGlobalDefines`.
5. The reserved uniform/global table was pruned (`0c94ec52d39`); re-add only fork-owned names that
   have a live CPU write or shader declaration. `hasUniform(U32)` applies to reserved indexed
   uniforms; hashed custom setters already no-op when their location is absent (`30221e9ba76`).
6. Texture filtering, wrapping, comparison, and sRGB decode are sampler-object choices at the C++
   bind site. The GLSL sampler type must agree with the chosen sampler, especially for PCSS raw depth
   versus comparison shadows.
7. Rigged object skinning is avatar-local (`aff85c50ddb`); absolute matrix helpers are removed.

Risk: shader-drift/API-contract, high. Confidence: high.

## Fork-added shader inventory

### Prism lens

| Files | Interface result | Required action | Risk / confidence |
|---|---|---|---|
| `class1/deferred/prismLensF.glsl`, `prismLensV.glsl` | Feature-owned lens uniforms plus standard model-view/projection/normal matrices. No forward light array and no direct depth compare. The fork vertex shader still declares loose matrix uniforms. | Replace the loose matrix declarations in `prismLensV.glsl` with upstream's `//[ENGINE_BLOCK Matrices]` splice point and set the matching matrix feature before creation; preserve custom uniform hashes/writes. No `UB_LIGHTS` or explicit `REVERSE_Z` branch is needed. Sample retained feeds with an explicit clamp sampler in C++. | medium / high |

Registration is near fork `llviewershadermgr.cpp:1378-1384`. The source itself is low risk; target
allocation and sampling are the higher-risk contract.

```glsl
// Shared matrix stack + derived matrices, spliced from
// class1/deferred/matricesBlock.glsl and bound at UB_MATRICES.
//[ENGINE_BLOCK Matrices]
```

### Actor ghost

| Files | Interface result | Required action | Risk / confidence |
|---|---|---|---|
| `class1/interface/actorghostF.glsl` | `ghostUseVertexAlpha` is feature-owned and still live; C++ sets it at `llactormover.cpp:4780,4913,5230`. | Preserve declaration, multiply path, and resets. The hashed setter safely ignores fallback variants with no location. | medium / high |
| `class1/interface/actorghostV.glsl` | Calls removed `getObjectSkinnedTransform()` at `:44,55`. | Rewrite HAS_SKIN path with `getSkinBlend`, `skinTransformH`, and `skinDirection`; create rigged variant with `VARIANT_RIGGED`. | high / high |

Delete the cpp-local `gSkinnedActorGhostProgram` definition and references; it has no header
declaration. The fork draw code already prefers `gActorGhostProgram.mRiggedVariant`
(`llactormover.cpp:4693`).

### Velocity and motion blur

| Files | Interface result | Required action | Risk / confidence |
|---|---|---|---|
| `class1/deferred/velocityV.glsl`, `velocityAlphaV.glsl` | Depend on old current and previous absolute object-skin transforms. | Redesign around current `skin_origin`/`getSkinBlend` and matching `last_skin_origin`/`getLastSkinBlend`; update CPU previous palette to the same rebased layout. | very high / high design, medium runtime |
| `class1/deferred/velocityF.glsl`, `velocityAlphaF.glsl`, `velocityCameraF.glsl`, `velocityDebugF.glsl` | Consume velocity/depth/matrix inputs; no rigging helper themselves. | Port program features to matrix/deferred UBOs; audit depth reconstruction for `REVERSE_Z`; use named point/clamp sampling for velocity/depth buffers. | high / medium-high |
| `class1/deferred/avatarVelocityV.glsl` | Uses the classic avatar palette, not `objectSkinV`'s object-rig palette. | Keep palette contract, but migrate standard matrices to upstream block and verify previous-camera transform timing. | medium-high / medium-high |
| `class1/deferred/motionBlurF.glsl` | Screen-space depth/velocity consumer. | Use upstream depth reconstruction and explicit sampler choices; verify reverse-Z near/far rejection and velocity sign. | high / medium |

Fork registration is around `llviewershadermgr.cpp:3666-3687`. Replace
`make_rigged_variant()` with `createShader(VARIANT_RIGGED)`.

### Projector volumetrics and froxel shaders

Files:

- `class1/deferred/projectorVolumetricF.glsl`
- `class3/deferred/projectorVolumetricF.glsl`
- `class1/deferred/projectorVolumetricTemporalF.glsl`
- `class1/deferred/projectorVolumetricUpsampleF.glsl`
- `class1/deferred/projectorVolumetricBloomFeedF.glsl`
- `class1/deferred/froxelUtil.glsl`
- `froxelInjectF.glsl`, `froxelIntegrateF.glsl`, `froxelMediaF.glsl`
- `froxelTemporalF.glsl`, `froxelApplyF.glsl`, `froxelDebugF.glsl`

Interface result:

- These programs use feature-owned froxel/projector uniforms but also depend on shared deferred depth,
  matrices, environment/light state, and `shadowUtil`.
- Shadow sampling must match upstream's two-type contract: raw depth for PCSS and comparison sampler
  for lower tiers. A sampler2D/sampler2DShadow mismatch is a link/runtime error, not a quality tweak.
- Slice/depth reconstruction and temporal rejection are reverse-Z sensitive.

Required action:

1. Keep upstream `deferredUtil.glsl` and `shadowUtil.glsl` includes/interfaces.
2. Declare the shader features that attach `UB_DEFERRED`, `UB_MATRICES`, `UB_ENVIRONMENT`, and lights
   actually consumed by injection/media passes.
3. Use shared `sampleSpotShadow` rather than local target comparison state.
4. Add or verify explicit `REVERSE_Z` handling for froxel slicing, depth bounds, temporal rejection,
   and upsample edge tests.
5. Choose named clamp samplers for screen/froxel targets and wrap samplers for periodic noise.

Risk: shader-drift/API-contract, high. Confidence: medium-high.

### Volumetric light

Files: `class1/deferred/volumetricLightF.glsl`, `class3/deferred/volumetricLightF.glsl`.

These consume sunlight/environment, deferred depth, and shadows. Load them with the corresponding
upstream shader features so they receive the environment, deferred, matrices, and lighting blocks.
Do not recreate the removed scalar/vector globals. Rebase any local shadow helper onto upstream
`shadowUtil`; audit ray depth and termination under reverse-Z.

Risk: shader-drift, high. Confidence: medium-high.

### Weather

Files:

- `class1/deferred/weatherRainF.glsl`
- `class1/deferred/weatherRainUpsampleF.glsl`
- `class1/deferred/weatherSurfaceF.glsl`
- `class1/deferred/weatherLightningF.glsl`

Interface result:

- Rain exposure uses `weather_rain_occlusion_matrix` and depth range/bias/softness controls; direct
  clip/depth comparisons are reverse-Z sensitive.
- Surface/upsample programs consume screen/deferred inputs and need upstream matrix/deferred blocks.
- Noise and mask sources need explicit sampler wrap/filter choices.

Required action: define one documented occlusion-depth convention. Prefer rendering the occlusion map
with upstream reverse-Z projection and comparing under `#ifdef REVERSE_Z`; alternatively convert to a
linear depth before storage/comparison. Do not retain an implicit forward `clip.z` test.

Risk: shader-drift/API-contract, high. Confidence: medium.

### Visible-diffuse and support shaders

| Files | Result and action | Risk / confidence |
|---|---|---|
| `class1/deferred/visibleDiffuseSeedF.glsl` | Port to upstream MRT/attachment layout; bind screen/deferred sources with explicit point/clamp and preserve sRGB decode intent. Guard publication when the sidecar attachment is absent. | high / medium-high |
| `class1/deferred/LPMUtil.glsl` | Treat as feature-owned tone/output utility; diff against upstream tonemap/color-space changes before including. Avoid duplicating corrected upstream output transforms. | medium-high / medium |

## Shared shaders changed on both sides

These files must start from upstream. The fork version must never win wholesale.

### `class1/avatar/objectSkinV.glsl`

Upstream wins. It defines `skin_origin`, `getSkinBlend`, `skinDirection`, `skinPoint`, and
`skinTransformH`. Reapply the fork previous-frame capability as a parallel rebased contract, not by
restoring `getObjectSkinnedTransform`/`getLastObjectSkinnedTransform`.

Risk: API-contract/shader-drift, very high. Confidence: high.

### `class1/deferred/deferredUtil.glsl`

Upstream wins for depth reconstruction, projection, probe/deferred blocks, and renderer fixes.
Reapply only committed, narrowly audited helper additions used by weather/froxel/sidecar. The
`proj_cookie_region`, `proj_cookie_orient`, and `projCookieUv()` interface is stash-only and excluded
from the committed port.

Risk: shader-drift, high. Confidence: high.

### `class1/deferred/fullbrightF.glsl`

Keep upstream lighting/color/alpha corrections. Replay only proven fork outputs or sidecar behavior,
ensuring attachment writes match the active MRT layout.

Risk: textual/shader-drift, medium-high. Confidence: medium-high.

### `class1/deferred/shadowUtil.glsl`

Upstream wins completely for gather PCF, receiver-plane depth bias, PCSS, sampler type, and
reverse-Z comparison (`e8b487a8160`). Reapply only a call adapter if a fork program needs one; do not
fork the algorithm.

Risk: shader-drift, very high. Confidence: high.

### `class1/deferred/tonemapUtilF.glsl`

Keep upstream tone/color corrections. Diff `LPMUtil` and 10-bit/ReShade additions at the function
level; one output transform owns transfer/gamut conversion to avoid double tonemapping.

Risk: shader-drift, high. Confidence: medium-high.

### `class2/deferred/alphaF.glsl` and `pbralphaF.glsl`

Keep upstream legacy-alpha lighting and corrected PBR/AO/punctual behavior (`03c0860`,
`12dc96aa56f`, `ed2558c`, `9517e0af79e`). Replay committed interleaved and sidecar deltas only;
projector-cookie remapping is optional stash work.
Forced mask remains a C++ legacy-material cutoff and must not spill into GLTF.

Risk: shader-drift, very high. Confidence: high.

### `class3/deferred/reflectionProbeF.glsl`

Keep upstream SH coefficient reconstruction, probe ordering/weights, and lifecycle fixes
(`59601e8eba7`, `3e6ec2e3f41`, `c343b4a3b93`). Then replay only the committed
`uniform int prism_auxiliary` and its four containment sites: the PBR SSR condition, PBR hero tap,
legacy SSR condition, and legacy pair of hero taps. In parallel, set staged
`ReflectionProbeData::iterationCount` and `glossySampleCount` to zero; the fork's old per-bind float
uniform uploads no longer control upstream's block-backed fields. This is a narrow delta on the
upstream SH shader, not permission to copy the fork shader wholesale.

```glsl
#if defined(SSR)
if (prism_auxiliary == 0 && cube_snapshot != 1 && glossiness >= 0.9)
{
    // complete upstream PBR SSR body
}
#endif
if (prism_auxiliary == 0)
{
    tapHeroProbe(glossenv, pos, norm, glossiness);
}

#if defined(SSR)
if (prism_auxiliary == 0 && cube_snapshot != 1)
{
    // complete upstream legacy SSR body
}
#endif
if (prism_auxiliary == 0)
{
    tapHeroProbe(glossenv, pos, norm, glossiness);
    tapHeroProbe(legacyenv, pos, norm, 1.0);
}
```

Risk: shader-drift/API-contract, very high. Confidence: high.

### `class3/deferred/spotLightF.glsl`

Keep upstream PCF/PCSS and corrected PBR light integration. Do not add cookie hooks in the committed
slice; if the stash experiment is accepted, add its narrow call-site remap while preserving upstream
sampler-type defines.

Risk: shader-drift, high. Confidence: high.

### Water shaders

Files: `class1/environment/waterF.glsl`, `class3/environment/underWaterF.glsl`,
`class3/environment/waterF.glsl`.

Keep upstream reflection, sampler, and environment-block changes. Replay fork sidecar/weather/10-bit
outputs only after checking MRT locations and color space. Water clipping/depth code must use the
upstream reverse-Z convention.

Risk: shader-drift, high. Confidence: medium-high.

## CPU-side program registration audit

- Remove the fork-populated body of `LLViewerShaderMgr::finalizeShaderList()` at
  `llviewershadermgr.cpp:430-532`, but keep upstream's assertion-only function shell and
  `llassert(no_redundant_shaders())`.
- Remove `make_rigged_variant()` usage for fork programs. Upstream examples use
  `createShader(VARIANT_RIGGED)` throughout, including highlight at upstream
  `llviewershadermgr.cpp:3614`.
- Reapply custom load blocks (Prism near fork `:1378`, visible diffuse near `:2475`,
  volumetric/projector programs near `:3116`, actor ghost near `:4352`) onto upstream load phases.
- Land each registration/load block in the same vertical slice as every GLSL file it references.
  Registration-first commits create guaranteed intermediate shader-load failures even when the final
  tree would be valid.
- For each program, set feature flags before `createShader()` so block declarations and shared
  includes agree with the linked source.
- Preserve custom uniform hashes only when `rg` finds both a GLSL declaration and a CPU writer or the
  uniform is intentionally shader-only. Drop dead reserved additions.

Risk: API-contract/shader-drift, high. Confidence: high.

## Per-program static audit checklist

Before a shader is allowed into an implementation commit:

- [ ] No removed `getObjectSkinnedTransform` helper remains.
- [ ] No removed individual light/environment/deferred global is redeclared as a workaround.
- [ ] Required upstream UBO feature flags are set.
- [ ] Every depth reconstruction/compare is classified as linear, forward clip, or reverse-Z.
- [ ] Shadow GLSL sampler type agrees with the C++ named sampler for every quality variant.
- [ ] Every sampled custom target has an explicit filter/wrap/sRGB-decode choice.
- [ ] Rigged variant uses `createShader(VARIANT_RIGGED)` and `mRiggedVariant`.
- [ ] `hasUniform()` is used only with reserved `U32` indices; hashed custom writes use the safe
  missing-location setter.
- [ ] MRT output locations exist in the active upstream target.
- [ ] Shared PBR, probe, shadow, and tonemap source is upstream-led.

## Runtime visualizations required later

- Velocity debug: static camera, moving camera, moving clone, 100x/150x clone, rigged attachment.
- Shadow tiers: off/PCF/PCSS in main and VCam, with projector alignment and soft-penumbra motion.
  Add cookie-atlas alignment only for the optional stash slice.
- Probe debug: main versus VCam PBR, cube-snapshot frame, hero/SSR enabled in main.
- Depth debug: weather occlusion, froxel slice bounds, temporal rejection, water clip, DoF/motion blur.
- Sidecar debug: attachment layout and visible-diffuse values for legacy alpha, PBR alpha, water, and
  clone draws.

Overall shader-port confidence: high for identified contract changes; medium until compilation and
the depth/motion/probe visualizations are run by the later build/test phase.

## Concrete shader and registration adaptations

### Program feature declarations

Custom deferred programs must state which upstream modules/UBOs they consume before creation. A
projector-volumetric program that reconstructs deferred position and samples shadows should follow
this shape:

```cpp
gProjectorVolumetricProgram.mName = "Projector Volumetric";
gProjectorVolumetricProgram.mShaderFiles.clear();
gProjectorVolumetricProgram.mShaderFiles.emplace_back(
    "deferred/postDeferredV.glsl", GL_VERTEX_SHADER);
gProjectorVolumetricProgram.mShaderFiles.emplace_back(
    "deferred/projectorVolumetricF.glsl", GL_FRAGMENT_SHADER);

gProjectorVolumetricProgram.mFeatures.isDeferred = true;
gProjectorVolumetricProgram.mFeatures.hasShadows = true;
gProjectorVolumetricProgram.mFeatures.hasAtmospherics = true;
gProjectorVolumetricProgram.mFeatures.hasGamma = true;
gProjectorVolumetricProgram.mFeatures.hasSrgb = true;
gProjectorVolumetricProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

success = gProjectorVolumetricProgram.createShader();
```

Only set `hasReflectionProbes` or `hasLighting` if the source actually calls those modules. Treat the
block above as a dependency declaration, not a standard flag bundle to paste onto every program.

Adversarial review:

- `hasShadows=true` without the matching shared shadow source/defines can change the GLSL sampler type
  while the fragment source still declares its own incompatible sampler.
- `hasSrgb=true` does not decide whether a particular texture bind uses hardware decode; the C++
  `ALSampler::SRGBDecode` bit still owns that read-time decision.
- Enabling unused blocks grows program interfaces and UBO traffic; disabling a used one can still
  link if the fork shader redeclares a stale global, producing silently divergent values.

Risk: shader-drift/API-contract. Confidence: medium-high.

### Reverse-Z-safe deferred position/depth use

Do not reconstruct view position from raw depth with a hardcoded `[0,1] -> [-1,1]` remap. Import
upstream `deferredUtil.glsl` and use its helpers:

```glsl
float ndcZFromScreenDepth(float depth);
bool isFarDepth(float depth);
float linearDepth(float depth, float znear, float zfar);
vec4 getPositionWithDepth(vec2 screen_uv, float depth);
```

Weather/froxel example:

```glsl
float raw_depth = texture(depthMap, screen_uv).r;
if (isFarDepth(raw_depth))
{
    discard;
}

vec3 view_position = getPositionWithDepth(screen_uv, raw_depth).xyz;
float view_distance = linearDepth(raw_depth, clip_range.x, clip_range.y);
```

If an occlusion map stores raw shadow depth instead of screen depth, give it a separate helper and
document its projection. Do not pass it to `linearDepth()` unless it uses the same near/far and depth
convention.

Adversarial review:

- Under reverse-Z, far/sky is zero. A legacy `depth >= 1.0` test makes sky look like near geometry.
- Applying `2*d-1` before `ndcZFromScreenDepth()` remaps twice in forward-Z and incorrectly in
  reverse-Z.
- Mixing linear distance from one camera with raw depth from another is especially likely in VCam
  and weather occlusion; the math can look plausible near the origin and fail at range.

Risk: shader-drift. Confidence: high.

### Weather rain occlusion comparison

Prefer comparing linear distances so the result is independent of forward/reverse storage:

```glsl
vec4 rain_clip = weather_rain_occlusion_matrix * vec4(world_position, 1.0);
vec3 rain_ndc = rain_clip.xyz / rain_clip.w;
vec2 rain_uv = rain_ndc.xy * 0.5 + 0.5;

float stored_raw = texture(weather_rain_occlusion_depth, rain_uv).r;
float stored_distance = linearDepth(stored_raw,
                                    weather_depth_range.x,
                                    weather_depth_range.y);
float receiver_distance = -rain_view_position.z;

float delta = receiver_distance - stored_distance - weather_depth_bias;
float exposure = 1.0 - smoothstep(0.0, weather_depth_softness, delta);
```

This code is valid only if the occlusion target is rendered with the upstream perspective/depth
convention and `rain_view_position` is in that same occlusion-camera space. For an orthographic rain
map, use an orthographic linearization instead.

Adversarial review:

- Treating `rain_ndc.z` as a linear receiver distance is wrong for perspective projection.
- `weather_depth_bias` units must match the chosen distance space. A legacy normalized-depth bias can
  become enormous in meters.
- UV outside `[0,1]`, `rain_clip.w <= 0`, and far/unwritten samples need explicit exposed/occluded
  policy; clamp sampling alone is not a policy.

Risk: shader-drift/API-contract. Confidence: medium pending projection audit.

### Shadow sampler declaration and bind must be one decision

The shared shader should own the type switch:

```glsl
#if SHADOW_PCSS
#define AL_SHADOW_SAMPLER sampler2D
#else
#define AL_SHADOW_SAMPLER sampler2DShadow
#endif

uniform AL_SHADOW_SAMPLER shadowMap4;
uniform AL_SHADOW_SAMPLER shadowMap5;
```

Use `#if`, not `#ifdef`: upstream defines `SHADOW_PCSS` to `0` for the comparison-sampler variant, so
`#ifdef` would incorrectly choose raw `sampler2D` in both variants.

And C++ must select on the exact setting that injected `SHADOW_PCSS`:

```cpp
static LLCachedControl<U32> shadow_quality(
    gSavedSettings, "AlchemyRenderShadowFilterQuality", 1);
const bool pcss = llclamp(shadow_quality(), 0u, 3u) >= 3;
const U32 sampler = gGL.getSampler(
    pcss ? ALSamplers::PointClamp : ALSamplers::ShadowCompare);
```

Custom projector/froxel programs should call upstream `bindShadowMaps()` rather than reproduce the
loop.

Adversarial review:

- `sampler2D` with a compare sampler and `sampler2DShadow` with a raw sampler are both undefined even
  if the texture format is correct.
- Compiling at quality 3 and temporarily rendering a probe at quality 0 must not change the sampler
  type. The probe path reduces work inside the same compiled shader.
- A custom program that names only spot maps still participates in upstream stale compare-unit
  cleanup; bypassing it can poison later material units.

Risk: shader-drift/API-contract. Confidence: high.

### Optional stash-only Prism cookie UV delta

This is not present in committed `47af3df7b1a`. If `stash@{0}` is separately accepted after the
committed port passes, preserve its exact ABI and operation order:

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

For both uniforms `.xy` is scale and `.zw` is offset; identity is `(1,1,0,0)`. C++ uploads both via
`uniform4fv`, using `orient_scale/orient_offset` and `region_scale/region_offset`. Orientation is
applied first, then the atlas region. Do not reinterpret orientation as an integer enum.

Adversarial review:

- Applying the remap before perspective divide or to shadow depth coordinates misaligns cookie and
  penumbra.
- A stale transform from a previous feed can crop ordinary projectors. CPU must upload
  `(1,1,0,0)` for both vectors on non-feed projectors.
- The clamp/wrap sampler for the cookie determines whether pixels outside the logical feed rectangle
  leak stale retained data.

Risk: shader-drift. Confidence: medium-high.

### Actor-ghost avatar-local vertex code

```glsl
#ifdef HAS_SKIN
mat3x4 getSkinBlend();
vec3 skinDirection(mat3x4 blend, vec3 direction);
vec4 skinTransformH(mat3x4 blend, vec3 position, mat4 transform);
#endif

#ifdef HAS_SKIN
mat3x4 blend = getSkinBlend();
vec4 view_position = skinTransformH(blend, position, modelview_matrix);
vec3 view_normal = normalize(mat3(modelview_matrix) * skinDirection(blend, normal));
#else
vec4 view_position = modelview_matrix * vec4(position, 1.0);
vec3 view_normal = normalize(mat3(modelview_matrix) * normal);
#endif
```

Keep `ghostUseVertexAlpha` in the fragment program:

```glsl
if (ghostUseVertexAlpha != 0)
{
    color.a *= vertex_color.a;
}
```

Risk: shader-drift. Confidence: high.

### Avatar-local velocity code

The current and previous helpers must have identical coordinate contracts:

```glsl
mat3x4 current_blend = getSkinBlend();
mat3x4 previous_blend = getLastSkinBlend();

vec4 current_view = skinTransformH(current_blend,
                                   position,
                                   modelview_matrix);
vec4 previous_view = lastSkinTransformH(previous_blend,
                                        position,
                                        last_modelview_matrix);

gl_Position = projection_matrix * current_view;
vary_cur_clip = projection_matrix_unjittered * current_view;
vary_last_clip = last_projection_matrix_unjittered * previous_view;
```

Add a reserved `LAST_SKIN_ORIGIN` entry mapped to `"last_skin_origin"`. The CPU upload must send
`AVATAR_MATRIX` from current packed offset 4, `SKIN_ORIGIN` from current offset 0,
`AVATAR_LAST_MATRIX` from previous offset 4, and `LAST_SKIN_ORIGIN` from previous offset 0. The full
CPU code and failure cases are specified in `MERGE_CLONES_ADAPTATION.md`.

Risk: shader-drift/API-contract. Confidence: medium-high.

### Sidecar MRT output guard

Shared alpha/PBR shader deltas should use an explicit compile define, not assume attachment presence:

```glsl
layout(location = 0) out vec4 frag_color;

#ifdef HAS_VISIBLE_DIFFUSE_SIDECAR
layout(location = SL_SIDECAR_ATTACHMENT) out vec4 visible_diffuse;
#endif

void publishVisibleDiffuse(vec4 value)
{
#ifdef HAS_VISIBLE_DIFFUSE_SIDECAR
    visible_diffuse = value;
#endif
}
```

The implementation must ensure `SL_SIDECAR_ATTACHMENT` is a compile-time numeric define shared with
the C++ target layout.

Adversarial review:

- Declaring an output for a missing attachment can make the FBO/pass incomplete or discard data,
  depending on driver and draw-buffer state.
- Enabling the attachment for a shader variant that never writes it leaves undefined values that
  blend into the sidecar.
- PBR and legacy diffuse meanings differ; define the sidecar's color space and whether alpha is
  coverage, material alpha, or post-light opacity.

Risk: shader-drift/API-contract. Confidence: medium.

## Adversarial shader review matrix

| Mutation to try during review | Correct result | Bug it exposes |
|---|---|---|
| Toggle reverse-Z while rendering sky/far pixels | Same reconstructed view position and far rejection | Hardcoded forward depth remap |
| Toggle shadow tier 2 <-> 3 | Program relinks/switches with matching sampler type | Independent compile/bind decisions |
| Move camera but freeze object | Velocity contains camera motion only | Jitter or wrong previous model-view |
| Move rigged limb but freeze root | Local limb velocity, no region-scale spikes | Previous palette not rebased |
| Optional stash slice: enable VCam after normal projector | Identity `(1,1,0,0)` for both cookie vectors never leaks | Stale cookie region/orientation |
| Disable sidecar while keeping alpha/PBR draws | Color output remains correct and no invalid MRT writes | Unconditional extra output |
| Sample sRGB feed through decode on/off variants | Exactly one transfer conversion | Sampler and shader both decode |
