# Lens Looks — shader/C++ contract (Fable, 2026-10-09)

Design: `C:\Users\xianw\cxs\lens_looks_design.md` (Opus). Three looks ship in one
batch with Rig Rim Quality (`doc/RIGRIM_QUALITY_CONTRACT.md`):

- **Infrared film** (B&W + Aerochrome), renderer-aware through the G-buffer, fused
  into the existing on-lens draw.
- **Soft-Edge / Sweet Spot**: Diffusion (default; pyramid blur composite) and
  Defocus (CoF injection → real DoF bokeh; needs DoF on).
- **Colour grads**: a tint on the Graduated ND wedge and a full-frame 85/80-series
  warming/cooling multiply, fused into the on-lens draw.

Approved defaults: no auto-enable of the avatar tag (one-click button + skin
fallback); B&W IR does **not** monochrome the post-on-lens additions (shafts,
froxel fog, Atmosphere-layer Roto Ink, projector volumetrics are composited
after `applyOnLensFilters` and stay coloured — documented v1 limitation);
Defocus ships, Diffusion is the default blur type. Mist/Streak are out of scope.

---

## 1. Shader files (written)

| File | Change |
|---|---|
| `class1/alchemy/onLensFiltersF.glsl` | `lensGradNDMask` / `lensGradTint` / `lensFrameFilter` (colour grads), `irClassify` / `irGain` / `irWater` / `irApply` (Infrared) under `#ifndef ON_LENS_NO_IR`; main order Night Mask → GradND → grad tint → Polarizer → frame filter → IR. Every addition early-outs on its own uniform (amount/strength 0 ⇒ identity before any fetch). |
| NEW `class1/alchemy/lensSoftEdgeF.glsl` | Soft-Edge composite (premultiplied over the screen; discards inside the sweet spot). |
| `class1/deferred/cofF.glsl` | gated Defocus CoF injection (`dof_softedge.x > 0`), original statements untouched. |

`postEffectUtilsF.glsl` is **unchanged** (it links into the 8 colorCorrect
variants). The grad-tint mask mirrors `applyGradND`'s wedge statements inside
`onLensFiltersF.glsl` and re-declares the three `gradnd_*` uniforms there
(separate compile unit, identical types — legal; keep the two in sync).

Vertex shader for the two post programs: the existing `deferred/postDeferredNoTCV.glsl`.
The blur pyramid is built with the **existing** bloom downsample programs
(`gBloomDownsampleFirstProgram` level 0 with Karis, `gBloomDownsampleProgram`
below) — no new pyramid shader.

---

## 2. Reserved uniforms — `llshadermgr.h` / `llshadermgr.cpp`

Only the samplers are reserved (they need `bindTexture`); every scalar/vector
uniform below is an `LLStaticHashedString` (`static LLStaticHashedString s_on_lens_ir("on_lens_ir");`
`shader.uniform4f(s_on_lens_ir, ...)`), as the design requires.

Append **after** the Rig Rim block (`RIG_RIM_QUALITY … RIG_RIM_WRAP_SRC`, 8 entries) and
before `END_RESERVED_UNIFORMS`; same order in the `push_back` list. Combined new
count = 14.

| # | enum | name string | bound to |
|---|---|---|---|
| 9 | `ON_LENS_ALBEDO` | `"onLensAlbedoMap"` | `mRT->deferredScreen` texture 0 (GL_SRGB8_ALPHA8 → sampled linear) |
| 10 | `LENS_PYRAMID0` | `"lensPyramid0"` | `mLensPyramid[0]` (1/2) |
| 11 | `LENS_PYRAMID1` | `"lensPyramid1"` | `mLensPyramid[1]` (1/4) |
| 12 | `LENS_PYRAMID2` | `"lensPyramid2"` | `mLensPyramid[2]` (1/8) |
| 13 | `LENS_PYRAMID3` | `"lensPyramid3"` | `mLensPyramid[3]` (1/16) |
| 14 | `LENS_PYRAMID4` | `"lensPyramid4"` | `mLensPyramid[4]` (1/32) |

Existing reserved entries the on-lens program now also needs bound:
`NORMAL_MAP "normalMap"` → deferredScreen texture 2 (`TFO_POINT`),
`DEFERRED_SPECULAR "specularRect"` → deferredScreen texture 1 (`TFO_POINT`),
`NIGHT_MASK_INV_PROJ "night_mask_inv_proj"` (upload whenever IR is active, not
only for Night Mask), plus today's `DEFERRED_DIFFUSE` (scene), `DEFERRED_DEPTH`,
`EXPOSURE_MAP`, `EXPOSURE`.

### Static-hashed uniforms and packing

```
-- colour grads (onLensFiltersF) -----------------------------------------------
on_lens_grads    xyz ND-wedge tint multiplier (linear, luminance-normalised), w amount 0..1 (0 = off)
on_lens_grads2   xyz full-frame 85/80 gain   (linear, luminance-normalised), w amount 0..1 (0 = off)

-- infrared (onLensFiltersF, compiled out under ON_LENS_NO_IR) -----------------
on_lens_ir       x strength 0..1 (0 = off), y mode (0 B&W, 1 Aerochrome), z exposure gain = 2^ExposureEV, w red leak 0..1
on_lens_ir2      x foliage gain = 2^FoliageEV, y sensitivity 0..1, z dry vegetation 0..1, w sky darken (stops)
on_lens_ir3      x water darken (stops), y skin gain = 2^SkinEV, z shadow depth 0..1, w halation 0..1
on_lens_ir4      x Aerochrome b, y Aerochrome g, z Aerochrome saturation, w debug view (0 off, 1 classes, 2 NIR gain, 3 confidence)
on_lens_ir5      x water detection 0/1, y avatar tag usable 0/1, z camera underwater 0/1, w reserved
on_lens_ir_up    xyz world +Z in VIEW space (unit; rotate (0,0,1) by the gGLLastModelView rotation), w water height − camera height (m, agent frame)

-- soft edge (lensSoftEdgeF; cofF reads lens_softedge, lens_softedge2, lens_softedge5, dof_softedge) --
lens_softedge    x shape (0 circle, 1 oval, 2 band), y size (fraction of frame height), z aspect (oval x/y), w angle (rad)
lens_softedge2   xy centre (uv, y-up), z feather, w blur 0..1 (0 in Defocus mode)
lens_softedge3   x glow, y glow threshold (exposed luma), z desaturate 0..1, w tint amount 0..1
lens_softedge4   xyz tint (linear), w show mask (1 = debug)
lens_softedge5   xy frame size px (all tiles: actorFxScreenSize()), zw this tile's offset px (actorFxFragOffset())
lens_softedge6   x exposure scale (exposureMap * RenderExposure) for the glow threshold, y mask gain 0..1 (1), zw reserved
dof_softedge     x edge CoF as a fraction of max_cof, 0..1 (0 = off, bit-exact) = RenderSoftEdgeBlur uploaded DIRECTLY;
                 yzw reserved and IGNORED by the shader.
                 (Changed after review P2-3: the earlier x = y = Blur packing made the edge CoF proportional to Blur^2.
                  Upload (Blur, 0, 0, 0) when Defocus is active, (0, 0, 0, 0) otherwise.)
```

Aerochrome filter table → `on_lens_ir4.xy` (`RenderInfraredAeroFilter`):

| index | filter | b | g |
|---|---|---|---|
| 0 | #12 (yellow) | 0.03 | 0.00 |
| 1 | #21 (orange) | 0.03 | 0.35 |
| 2 | #25 (red) | 0.03 | 0.80 |
| 3 | none | 0.60 | 0.00 |

Colour-grad gains: `gain = ALEnvIntensity::kelvinRatio(T_out)` divided by its
`linearLuminance` (`alenvintensity.cpp` 155/160), with
`T_out = 1e6 / (1e6 / 5500 + mired)`; presets in mired: 85 +112, 85B +131,
85C +81, 81A +18, 80A −131, 80B −112, 80C −81, 82A −18, custom = slider. The
ND tint uses a swatch (sRGB → linear → luminance-normalised).

---

## 3. Programs (`llviewershadermgr.cpp`)

| program | change |
|---|---|
| `gOnLensFiltersProgram` (line 5256) | unchanged files/features (`isDeferred`, `hasPostEffects`). **Fallback:** if `createShader()` fails, `LL_WARNS`, `clearPermutations(); addPermutation("ON_LENS_NO_IR", "1");` and retry; on success set `gOnLensNoIR = true` (IR unavailable → Lightbox status "Infrared unavailable on this driver"; ND/Pol/Night Mask/grads keep working). Only if the retry also fails does `success` follow today's path. |
| NEW `gLensSoftEdgeProgram` | "Lens Soft Edge Shader", no features, `deferred/postDeferredNoTCV.glsl` + `alchemy/lensSoftEdgeF.glsl`, `mShaderLevel[SHADER_DEFERRED]`, soft-fail (`isComplete()` gates the stage). |
| `gDeferredCoFProgram` | unchanged registration; new uniforms are static-hashed and default to 0. |

---

## 4. Pass order and insertion points (`pipeline.cpp` @ e189340efa4, re-verified)

```
renderFinalize (22545 if(hdr)):
  updateNightMaskAnchor -> ALTron -> [ALLensLooks::resolveFrame()  NEW] -> generateLuminance (meter ramp, §4.4)
  -> generateExposure -> Tron world -> RotoInk camera
  -> applyOnLensFilters(&mRT->screen)   (22610)   : Night Mask -> [ReShade raw capture] -> GradND -> grad tint -> Pol -> frame filter -> IR  -> copy-back
  -> applyLensSoftEdge(&mRT->screen)    (NEW, before 22618): pyramid build -> composite
  -> generateBloomHDR                   (22618)
...
renderDoF (22384): gDeferredCoFProgram bind -> [dof_softedge / lens_softedge* upload  NEW]
```

### 4.1 `applyOnLensFilters` (14075–14408)

- **Early gates** (14101 and 14202): extend with `ir_active || grads_active`
  (`ir_active = RenderInfraredEnabled && !gOnLensNoIR && strength > 0`,
  `grads_active = tint_amount > 0 || frame_amount > 0`). With all four
  features off the function still returns before any GPU work — byte-identical to
  today.
- **Main-view guard** (design pre-existing hazard 2): add
  `if (screen != &mRT->screen || mRT != &mMainRT) return;` after the `no_post`
  return (14108). Note for the review: non-main `renderFinalize` callers
  (impostors, probes, GLTF preview) lose ND/Pol they should never have had.
- **Copy-back** (14373, design hazard 1): replace `screen->copyContents(...)` with
  `screen->copyColorToAttachment0(mWaterDis, 0, 0, w, h, 0, 0, w, h, GL_NEAREST)`
  (the helper `renderCineOutline` uses at 18250) so the sidecar / coverage / ALR
  parity attachments never receive the filtered beauty colour.
- **Per-draw uniforms** inside `draw_on_lens_pass(include_night, include_gradnd_pol)`:
  the Lens Looks ride the **second** draw (`include_gradnd_pol`, after the raw
  capture) so the ReShade capture stays the visible-light scene:
  ```
  const bool looks = include_gradnd_pol;
  uniform4f(s_on_lens_grads,  tint.rgb,  looks ? tint_amount  : 0)
  uniform4f(s_on_lens_grads2, gain.rgb,  looks ? frame_amount : 0)
  uniform4f(s_on_lens_ir,  looks && ir_active ? strength : 0, mode, exp2(ExposureEV), red_leak)
  uniform4f(s_on_lens_ir2, exp2(FoliageEV), sensitivity, dry, sky_darken)
  uniform4f(s_on_lens_ir3, water_darken, exp2(SkinEV), shadow_depth, halation)
  uniform4f(s_on_lens_ir4, aero_b, aero_g, aero_sat, debug_view)
  uniform4f(s_on_lens_ir5, water_detect, tag_usable, camera_underwater, 0)
  uniform4f(s_on_lens_ir_up, up_view.xyz, water_height - camera_z)
  if (looks && ir_active) {
      bindTexture(ON_LENS_ALBEDO,     &mRT->deferredScreen /*tex 0*/, false, TFO_POINT)   // via bindTexture(index 0)
      bindTexture(DEFERRED_SPECULAR,  &mRT->deferredScreen /*tex 1*/, false, TFO_POINT)
      bindTexture(NORMAL_MAP,         &mRT->deferredScreen /*tex 2*/, false, TFO_POINT)
      uniformMatrix4fv(NIGHT_MASK_INV_PROJ, inverse(gGLLastProjection))    // always when IR is active (irWater)
  }
  ```
  `tag_usable = LLViewerShaderMgr::gbufferAvatarTagActive()`.
  `up_view` = `glm::mat3(gGLLastModelView) * vec3(0,0,1)`; `water_height` and
  `camera_underwater` exactly as `renderTronWorld` computes them for
  `TRON_WATER` (18700); `water_detect = RenderInfraredWaterDarken > 0`.
  `camera_z` = camera agent-frame z.
  Uniform locations of the `on_lens_ir*` names are −1 on the `ON_LENS_NO_IR`
  fallback and the uploads are no-ops.
- GL state inside the draw: unchanged (`LLGLDepthTest depth(GL_FALSE, GL_FALSE)`,
  `LLGLDisable blend`, target `mWaterDis`).

### 4.2 `applyLensSoftEdge(LLRenderTarget* screen)` — NEW, called at 22617 (after `applyOnLensFilters`, before `generateBloomHDR`)

Gate: `RenderSoftEdgeEnabled && hdr && mRT == &mMainRT && screen == &mRT->screen && !gCubeSnapshot && !sPrismLensRender && !sImpostorRender && !sRenderingHUDs && colorCorrectWillApplyExposure(true) && gLensSoftEdgeProgram.isComplete() && gBloomDownsampleProgram.isComplete() && gBloomDownsampleFirstProgram.isComplete() && ensureLensPyramid()`.

```
LL_PROFILE_GPU_ZONE("lens soft edge pyramid");
{ LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);
  for i in 0..4:  src = (i == 0) ? screen : &mLensPyramid[i-1];  shader = (i == 0) ? First : Downsample
      mLensPyramid[i].bindTarget(); shader->bind();
      shader->bindTexture(DIFFUSE_MAP, src, false, TFO_BILINEAR);
      shader->uniform2f(BLOOM_TEXEL_SIZE, 1/src->getWidth(), 1/src->getHeight());
      mScreenTriangleVB->setBuffer(); drawArrays(TRIANGLES,0,3); mLensPyramid[i].flush(); shader->unbind(); }
LL_PROFILE_GPU_ZONE("lens soft edge");
{ screen->bindTarget();
  static const U32 guarded[] = { SL_SIDECAR_ATTACHMENT, SL_COVERAGE_ATTACHMENT /*, ALR_RIM_PARITY_ATTACHMENT when present*/ };
  LLScopedIndexedDrawBufferGuard att0_only(screen->getNumTextures() > 1, guarded, n);   // llrender.h 654
  gGL.setColorMask(true, false);                 // alpha (glow tag) preserved
  LLGLDepthTest depth(GL_FALSE, GL_FALSE); LLGLDisable cull(GL_CULL_FACE);
  LLGLEnable blend(GL_BLEND); gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_ONE_MINUS_SOURCE_ALPHA);   // premultiplied
  if (show_mask) { /* blend disabled instead, full colour mask */ }
  gLensSoftEdgeProgram.bind();
  bindTexture(LENS_PYRAMID0..4, &mLensPyramid[0..4], false, TFO_BILINEAR);
  uniform4f(lens_softedge..6 per §2; lens_softedge2.w = (BlurType == 0) ? Blur : 0)
  draw; unbind; screen->flush();
  gGL.setSceneBlendType(LLRender::BT_ALPHA); gGL.setColorMask(true, true); }
```
Anchor (`lens_softedge2.xy`): 0 manual (`RenderSoftEdgeCenterX/Y`), 1 DoF focus
point (`LLPipeline::sLastFocusPoint` projected with gGLLastModelView/Projection,
uv y-up), 2 Subject A head (`LLDirectorCast::instance().resolveSubjectA()`
head joint, projected); smoothed with a 0.15 s exponential ramp, falling back to
manual when the target is off-screen/invalid. Tile-aware mask:
`lens_softedge5 = (actorFxScreenSize(), actorFxFragOffset())`
(`lldrawpool.cpp` 722–759 pattern), so the sweet spot sits in the same place on
every tile of a hi-res snapshot; the pyramid itself is tile-local (documented
limitation: blur seams at tile borders).

### 4.3 Defocus — `renderDoF` (22384–22397)

At the `gDeferredCoFProgram` bind add:
```
const bool defocus = RenderSoftEdgeEnabled && RenderSoftEdgeBlurType == 1 && lens looks main-view predicate;
uniform4f(s_dof_softedge, defocus ? Blur : 0.f, 0.f, 0.f, 0.f);   // ALWAYS upload: the uniform persists in the program; x alone drives the edge CoF (linear in Blur)
if (defocus) upload lens_softedge, lens_softedge2 (centre/feather), lens_softedge5 as in §4.2
```
`dof_softedge.x == 0` is the bit-exact off path of `cofF.glsl`. When DoF is off
(`RenderDepthOfField` false, build mode, or `gCubeSnapshot`) Defocus cannot run:
show the signal hint "Defocus needs DoF" and let the composite run with
`lens_softedge2.w = 0` (glow / desaturate / tint only).

### 4.4 Exposure meter ramp (design: "Tron B1 pattern")

`ALLensLooks::resolveFrame()` once per frame next to `ALTron::resolveFrame()`
(22562): `mBloomScale` ramps (0.4 s time constant) toward 0 while
`ir_active || (soft-edge enabled && Glow > 0)`, else toward 1; fold it into the
`night_mask_bloom_scale` upload in `generateLuminance` (12578–12582) exactly as
the Tron scale is folded (product of the independent ramps, main view only).

### 4.5 Settings validity / off path

- All amounts 0 ⇒ `applyOnLensFilters` early-returns as today; CoF untouched;
  no pyramid allocated, no composite. When ND/Pol are on but the looks are off,
  the new functions return their input before any fetch ⇒ ND/Pol arithmetic is
  unchanged.
- Legacy gamma / no-post snapshots / non-HDR: behind the existing `no_post` and
  `hdr` gates ⇒ off.
- Never part of probe captures / Prism / impostors / HUD (main-view guard).

---

## 5. Targets

```
LLRenderTarget mLensPyramid[5];   // GL_R11F_G11F_B10F, screen/2 .. screen/32 (ceil), no depth; main RT only
bool ensureLensPyramid();  void releaseLensPyramid();
```
Lazy: allocate on first use at the current `mRT->screen` size (re-allocate on
size change), release when `RenderSoftEdgeEnabled` goes false (checked at the
§4.2 site) and on screen release/resize. ≈ 11 MB at 4K.

---

## 6. Settings (`settings_alchemy.xml`, next to `RenderGradND*` at line 1892 — all `Render*` on-lens settings live there)

### Infrared (`RenderInfrared*`)

| setting | type | default | range | → |
|---|---|---|---|---|
| `RenderInfraredEnabled` | BOOL | false | | gate |
| `RenderInfraredMode` | U32 | 0 | 0 B&W, 1 Aerochrome | `on_lens_ir.y` |
| `RenderInfraredStrength` | F32 | 1 | 0..1 | `on_lens_ir.x` |
| `RenderInfraredExposureEV` | F32 | 0 | −3..3 | `on_lens_ir.z = 2^v` |
| `RenderInfraredFoliageEV` | F32 | 0 | −2..3 | `on_lens_ir2.x = 2^v` |
| `RenderInfraredFoliageSensitivity` | F32 | 0.5 | 0..1 | `on_lens_ir2.y` |
| `RenderInfraredDryVegetation` | F32 | 0.4 | 0..1 | `on_lens_ir2.z` |
| `RenderInfraredSkyDarken` | F32 | 2 | 0..6 stops | `on_lens_ir2.w` |
| `RenderInfraredWaterDarken` | F32 | 2 | 0..6 stops | `on_lens_ir3.x`; `on_lens_ir5.x = v > 0` |
| `RenderInfraredSkinEV` | F32 | 0.5 | −2..2 | `on_lens_ir3.y = 2^v` |
| `RenderInfraredShadowDepth` | F32 | 0.4 | 0..1 | `on_lens_ir3.z` |
| `RenderInfraredRedLeak` | F32 | 0.1 | 0..1 | `on_lens_ir.w` |
| `RenderInfraredHalation` | F32 | 0.25 | 0..1 | `on_lens_ir3.w` |
| `RenderInfraredAeroFilter` | U32 | 0 | table §2 | `on_lens_ir4.xy` |
| `RenderInfraredAeroSaturation` | F32 | 1.15 | 0..2 | `on_lens_ir4.z` |
| `RenderInfraredDebugView` | U32 | 0 | 0..3 | `on_lens_ir4.w` |
| `RenderInfraredPresetSetsGrain` | BOOL | false | | preset action may set film grain |

Presets (action combo; values → the settings above): Wood 720nm Classic (B&W,
FoliageEV 0.5, SkyDarken 2.5, RedLeak 0.15, Halation 0.3), Kodak HIE (B&W,
FoliageEV 1.0, SkyDarken 3, RedLeak 0.05, Halation 0.6, grain hint), Subtle
665nm (B&W, Strength 0.85, FoliageEV 0, SkyDarken 1.2, RedLeak 0.35, Halation
0.1), Deep 850nm (B&W, FoliageEV 1.2, SkyDarken 4, RedLeak 0, Halation 0.2,
WaterDarken 3), Aerochrome #12 / Hot Pink #21 / Red #25 (mode 1, AeroFilter
0/1/2, saturation 1.15/1.3/1.1).

### Soft-Edge (`RenderSoftEdge*`)

| setting | type | default | range | → |
|---|---|---|---|---|
| `RenderSoftEdgeEnabled` | BOOL | false | | gate |
| `RenderSoftEdgeBlurType` | U32 | 0 | 0 Diffusion, 1 Defocus | `lens_softedge2.w` / `dof_softedge` |
| `RenderSoftEdgeShape` | U32 | 1 | 0 circle, 1 oval, 2 band | `lens_softedge.x` |
| `RenderSoftEdgeAnchor` | U32 | 0 | 0 manual, 1 DoF focus, 2 Subject A head | §4.2 |
| `RenderSoftEdgeCenterX` / `Y` | F32 | 0.5 | 0..1 | `lens_softedge2.xy` |
| `RenderSoftEdgeSize` | F32 | 0.35 | 0.05..1.5 | `lens_softedge.y` |
| `RenderSoftEdgeAspect` | F32 | 1.4 | 0.3..3 | `lens_softedge.z` |
| `RenderSoftEdgeAngle` | F32 | 0 | −90..90 deg | `lens_softedge.w` (rad) |
| `RenderSoftEdgeFeather` | F32 | 0.4 | 0.02..2 | `lens_softedge2.z` |
| `RenderSoftEdgeBlur` | F32 | 0.5 | 0..1 | `lens_softedge2.w` (Diffusion) / `dof_softedge.xy` (Defocus) |
| `RenderSoftEdgeGlow` | F32 | 0.2 | 0..2 | `lens_softedge3.x` |
| `RenderSoftEdgeGlowThreshold` | F32 | 0.8 | 0.1..4 | `lens_softedge3.y` |
| `RenderSoftEdgeDesaturate` | F32 | 0 | 0..1 | `lens_softedge3.z` |
| `RenderSoftEdgeTint` | Color3 | (1, 0.92, 0.8) | sRGB → linear | `lens_softedge4.xyz` |
| `RenderSoftEdgeTintAmount` | F32 | 0 | 0..1 | `lens_softedge3.w` |
| `RenderSoftEdgeShowMask` | BOOL | false | | `lens_softedge4.w` |

Presets (C++20 designated-initialiser rows): Vaseline Oval (oval, size .35,
aspect 1.4, feather .5, blur .6, glow .25, tint amount .3), Lensbaby Sweet
(circle, size .22, feather .35, blur .9, glow .1), Dream Sequence (oval, size
.3, feather .8, blur .5, glow .6, desaturate .3, tint amount .5), Faux Tilt
Band (band, size .18, angle 0, feather .4, blur .8), Vintage Portrait (oval,
size .4, feather .5, blur .4, glow .15, desaturate .2, tint amount .4),
Nightmare Edge (circle, size .28, feather .25, blur 1, glow .8, desaturate .8,
tint (0.6,0.7,1.0) amount .6).

### Colour grads

| setting | type | default | range | → |
|---|---|---|---|---|
| `RenderGradNDTintColor` | Color3 | (1,1,1) | sRGB | `on_lens_grads.xyz` (linear, lum-normalised) |
| `RenderGradNDTintAmount` | F32 | 0 | 0..1 | `on_lens_grads.w` (also 0 when `RenderGradNDEnabled` is off) |
| `RenderLensFilterPreset` | U32 | 0 | 0 custom, 1 85, 2 85B, 3 85C, 4 81A, 5 80A, 6 80B, 7 80C, 8 82A | mired table §2 |
| `RenderLensFilterMired` | F32 | 0 | −200..200 | custom mired shift |
| `RenderLensFilterAmount` | F32 | 0 | 0..1 | `on_lens_grads2.w` |

Not in graphics presets. UI per the design: Lightbox Image tab after Polarizer
("Infrared Film", "Soft Edge / Sweet Spot", grads inside the Graduated ND group
+ a "Lens Filter" row), Exposure hub Post tab compact rows + "Filters…";
signal hints "Skin needs avatar tag [Enable]" (when IR on and `tag_usable` false),
"Defocus needs DoF", "Infrared unavailable on this driver" (`gOnLensNoIR`).
`xui_layout_check.py` zero findings; `ALScrollFit` applies.

---

## 7. Debug-view encodings

IR (`RenderInfraredDebugView`, replaces the frame while active): 1 classes — R
chlorophyll score f, G skin score s, B sky/water; 2 NIR gain — grey = gain/4
(mid-grey = gain 2 ≈ +1 stop); 3 confidence — grey 0..1 (1 = G-buffer trusted,
0 = radiance-only fallback). Soft-Edge `ShowMask`: the mask as grey (blend off).

---

## 8. OFF-path invariant

- Grads: `on_lens_grads.w <= 0` and `on_lens_grads2.w <= 0` ⇒ `lensGradTint`/`lensFrameFilter` return the input before any fetch. IR: `on_lens_ir.x <= 0` ⇒ `irApply` returns the input before any fetch. `applyNightMask` / `applyGradND` / `applyPolarizer` are untouched (`postEffectUtilsF.glsl` unchanged) ⇒ with the looks off the on-lens result is bit-exact to today. The `ON_LENS_NO_IR` build is today's file plus the grads.
- CoF: `dof_softedge.x <= 0` ⇒ the injection branch is untaken; the original statements are unchanged (the mask function is only called inside the branch).
- Soft-Edge: not enabled ⇒ no pyramid, no draw.
- GL defaults: every new uniform is 0 when never uploaded, which is the off value for all of them.
- Proof: snapshot byte-compare with `RenderGradNDEnabled` on / looks off against the pre-batch exe (`AlchemyTest.pre-rimq.exe`, shared with the rim batch).

---

## 9. Perf targets and Tracy zones

Design estimates (5090): IR fused ≈ 0.035 ms @1440p / 0.08 ms @4K (alone 0.08 /
0.2), Soft-Edge ≈ 0.05 / 0.1 ms, halation free. GPU zones: existing
`"on-lens filters"` covers IR + grads; add `LL_PROFILE_GPU_ZONE("lens soft edge pyramid")`
and `("lens soft edge")`. CPU: `LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderFinalize - lens soft edge")`.

---

## 10. Shader-side notes for the reviewer

- Albedo is **not** manually sRGB-decoded: `deferredScreen` att0 is `GL_SRGB8_ALPHA8`
  (pipeline.cpp 1364), so `texture()` already returns linear — the same way every
  deferred light reads `gb.albedo`. Alpha of att0 = emissive/fullbright tag
  (pbropaqueF writes 0, sky/fullbright 1) → `gain = 1` for emissive.
- Legacy roughness = `1 − specularRect.a` (glossiness); PBR = `orm.g`, metallic `orm.b`;
  flag via `GET_GBUFFER_FLAG(normal.w, GBUFFER_FLAG_HAS_PBR)` (preamble macro).
- Avatar tag via `GBUFFER_AVATAR_TAG_OF(normal.w)` under `#ifdef GBUFFER_AVATAR_TAG`
  (global define), additionally gated by `on_lens_ir5.y`.
- Confidence = irradiance chroma spread of `I = L / max(A, 0.02)` (`1 − smoothstep(0.6, 2.5, spread)`)
  × black-albedo rejection; the radiance-only detector classifies `L / max(L)`
  as a pseudo-albedo with the same classifier; `f`, `s`, `gain` are blended by
  confidence. Forward-rendered alpha foliage/particles (no G-buffer) therefore
  fall to the radiance detector automatically.
- NIR = `lum(L) · gain · k_irr`, `gain = clamp(ρ / max(lum(A), 0.02), 0, 16)`, `k_irr = 1 − ShadowDepth · blueness(I) · conf`;
  sky `lum(L)·2^(−SkyDarken·blueness(L))`; water via `nightMaskGetPosition`
  (dedicated `night_mask_inv_proj`) + horizontal-normal + water-plane height,
  glints kept (`lum·exposure > 2..4`).
- Halation raises `diff.a` (glow tag) by `Halation · strength · smoothstep(1,4, lum·exposure) · max(f, 0.35)`;
  never lowers it.
- Soft-Edge composite: `m = smoothstep(1, 1+feather, d)` in frame-height units
  (circle / oval / band, rotated), `discard` inside; blur = two adjacent pyramid
  levels at `level = blur·m·4`, 3×3 tent each; glow from levels 3–4 with a soft
  knee at the exposed threshold; desaturate/tint scale with `m`; premultiplied
  `(c·a, a)`, `a = m` (Diffusion) or `m · max(desat, tint)` (Defocus: the DoF
  blur stays, the composite only adds glow/desat/tint).
- CoF injection: `sc_edge = dof_softedge.x · max_cof · m`, written as `sc = -sc_edge`
  only when it exceeds `|sc|` (never shrinks an existing blur of either sign).
  **Sign (review B1):** `calc_cof` gives far pixels a NEGATIVE `sc`
  (`focal_distance = -subject_distance`, `depth < 0`), and both DoF programs blur
  the far side on `sc < -0.5` (`postDeferredF.glsl`; `FRONT_BLUR` only adds the
  `sc > 0.5` near branch), so the negative sign is what makes Defocus work with
  `gDeferredPostProgramNoNear` (the default) and with near-blur on alike.
- Soft-Edge glow is emitted as a **separate additive term**: output
  `vec4(c·a + glow_add, a)` under ONE / ONE_MINUS_SRC_ALPHA, so the glow lands
  on the frame scaled by `m` once in both modes and is not gated by the Defocus
  coverage `a = m·max(desat, tint)` (review P1). Glow samples levels 3/4 with
  single bilinear taps; the budget outside the spot is 2×9 (blur tents) + 2
  (glow) = **20 taps** (was 36, review P2-8). All pyramid reads are
  `textureLod(…, 0)` (post-discard flow, review P2-6).

## 11. Risks for review

1. Texture bindings on the on-lens program (albedo/spec/normal) — `bindTexture(…, index)` must select the right `deferredScreen` texture index; verify with IR debug view 1 on an alpha-mask tree / grass / water / sky / tagged avatar (the design's discriminating test).
2. `night_mask_inv_proj` must be uploaded whenever IR is active (today only when Night Mask is active) or `irWater` reconstructs garbage.
3. Soft-Edge GL state in `renderFinalize` (blend func, colour mask, draw buffers) vs the beauty pass and sidecars — prove the off path is inert.
4. `dof_softedge` persists in `gDeferredCoFProgram`: upload 0 whenever Defocus is not active.
5. Reserved-uniform list positions (14 new, after the rim block).
6. `ON_LENS_NO_IR` retry must happen only after a real compile failure and must not reset `success` for the stock chain.
7. Pyramid tile-locality on tiled snapshots; B&W IR leaves later-composited elements coloured (documented).
