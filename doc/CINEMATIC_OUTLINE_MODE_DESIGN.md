# Cinematic Outline / Silhouette Render Mode — Design & Implementation Spec

**Feature:** Cine Outline (`CineOutline*`). **Target:** deferred post chain (`LLPipeline::renderFinalize`).
**Look:** subject rendered near-black by the dark set + Cine Light Rig; this feature adds a bright colored
contour tracing the full silhouette AND internal contour edges (arm over torso), fed into bloom so it glows.
Rim color washes (blue/red) stay the Light Rig's job — this feature only draws the line.

**Technique:** screen-space **edge detection** (Sobel; Roberts-cross cheap fallback) over the deferred
G-buffer **normal** + **depth**, combined (normal OR depth discontinuity) → colored line → **additively
composited into the linear HDR scene buffer BEFORE bloom extraction**, so the existing bloom pyramid blooms it
for free. Render-only, deterministic, zero cost when disabled.

## 1. Confirmed pipeline ground truth (file:line)

### G-buffer access — `app_settings/shaders/class1/deferred/deferredUtil.glsl`
- `getNorm(vec2)` :428 — decoded view-space normal `.xyz`, gbuffer flags `.w`; `getNormRaw` :434 (cheaper).
- `linearDepth(d,znear,zfar)` :441; `getDepth(vec2)` :452 (raw); `getPosition(vec2)` :570 — use
  `-getPosition(tc).z` as linear view depth (no znear/zfar uniforms; `inv_proj` already a deferredUtil uniform).
- Flags: `gbufferUtil.glsl:55`. Sky writes `GBUFFER_FLAG_SKIP_ATMOS` (`cloudsF.glsl:228`); avatars write
  `GBUFFER_FLAG_HAS_ATMOS` (`avatarF.glsl:55`, SHARED with prims — no free avatar-only bit).
- Sampler binding: `LLPipeline::bindDeferredShader` (`pipeline.cpp:16095`) binds `normalMap` (:16117) + `depthMap`
  (:16131). (Do NOT reuse `RenderEdgeDepthCutoff`/`RenderEdgeNormCutoff` at :16326 — those tune SSAO; give the
  outline its own sensitivities.)

### Post chain & bloom — `LLPipeline::renderFinalize` (`pipeline.cpp:15670`)
- HDR path (`RenderHDREnabled`, :15691): `generateLuminance` :15704 → `generateExposure` :15706 →
  **`generateBloomHDR(&mRT->screen)` :15714** (builds bloom from the linear HDR scene buffer); composite folded
  into `colorCorrect` :15745. `generateBloomHDR` :11825 thresholds via `RenderBloomThreshold` (1.0) and boosts
  the scene alpha channel into bloom via `RenderBloomAlphaGlowBoost` :11841.
- Legacy path: `generateGlow(&mRT->postPingMap)` :15753 (impl :11712) — `GLOW_MIN_LUMINANCE=9999` :11726 disables
  luminance extraction, so glow is **alpha-driven** (`glowExtractF.glsl:61`). `combineGlow` :15396 (call :15793).
- Precedents: additive feed into bloom `feedProjectorVolumetricBloom` :15022 (`BT_ADD` fullscreen tri into
  `mRT->bloomMip[0]`); full G-buffer post pass `renderVolumetric` :12329 (`bindDeferredShader`, `DEFERRED_SCREEN_RES`,
  `mScreenTriangleVB->drawArrays(TRIANGLES,0,3)`, disabled-path `copyRenderTarget`).

### Shader registration & settings idioms
- Program template `gVolumetricLightProgram` (`llviewershadermgr.cpp:3116-3136`): `isDeferred=true`, VS
  `deferred/postDeferredNoTCV.glsl` (fullscreen tri), `mShaderLevel[SHADER_DEFERRED]`; `mShaderList.push_back`
  near :450.
- Reserved uniforms: `llshadermgr.h` enum + matching `mReservedUniforms.push_back` in `llshadermgr.cpp`
  (pattern `"gobo_anim_params"` :1770; ENUM ORDER = PUSH ORDER).
- Settings: in-function `static LLCachedControl<>` (pattern `pipeline.cpp:15784`). UI precedent
  `panel_cine_light_rig.xml` / `llfloaterdirector.cpp`. Presentation-time (pulse): `FROXEL_TIME =
  fmod(presentation_time,3600)` `pipeline.cpp:13650`.

## 2. Edge-detection shader — new `class1/deferred/cineOutlineF.glsl` (VS: reuse `postDeferredNoTCV.glsl`)

Uniforms: `normalMap`, `depthMap`, `screen_res` (DEFERRED_SCREEN_RES), `outline_color` (linear RGB),
`outline_params` (x thickness_px, y depth_sens, z normal_sens, w intensity HDR mult),
`outline_params2` (x glow_alpha, y max_distance, z pulse_amount, w pulse_phase precomputed CPU-side).

- **8-tap Sobel ring** (thickness = texel spacing `o = thickness/screen_res`): 18 fetches of 2 textures, no
  dependent reads. (Roberts-cross fallback = 4 diagonal taps / 10 fetches; ship Sobel, keep Roberts as #define.)
- **Depth edge** (relative, grazing-robust): `gD = length(sobel(z))/max(zc,0.01)`; `facing = clamp(N·V,0.1,1)`;
  `eD = smoothstep(t0/facing, t1/facing, gD*depth_sens)`, `t0=0.03,t1=0.10`.
- **Normal edge:** `gN = sqrt(dot(nx,nx)+dot(ny,ny))*0.25`; `eN = smoothstep(0.4,1.0,gN*normal_sens)`.
- **Combine + sky reject:** `edge = 1-(1-eD)*(1-eN)`; if center `raw_d>=0.999999` or center SKIP_ATMOS flag →
  `edge=0` (silhouette lands 1px inside the subject; empty bg = free). Clamp neighbor `z_i = min(z_i, zc+100)`
  so far-plane delta saturates (outer silhouette edge) instead of exploding the gradient.
- Output: `frag_color = vec4(outline_color*intensity*edge, edge*glow_alpha)` (alpha = bloom/glow feed).
- Phase 1 clamp thickness [0.5,3.0] (spacing double-lines beyond ~3px; Phase 2 dilate/12-tap if wider needed).

## 3. Pipeline integration

**New pass `LLPipeline::renderCineOutline(LLRenderTarget* dst)`** — additive in place, modeled on
`renderVolumetric` + `feedProjectorVolumetricBloom` blend state. NO ping-pong (additive in place → disabled
path is a true zero-cost no-op, early-return before any GL state):
```cpp
static LLCachedControl<bool> enabled(gSavedSettings,"CineOutlineEnabled",false);
if (!enabled || gCubeSnapshot || gSnapshotNoPost || !gCineOutlineProgram.isComplete()) return;
LLGLDepthTest depth(GL_FALSE); LLGLEnable blend(GL_BLEND); LLGLDisable cull(GL_CULL_FACE);
gGL.setSceneBlendType(LLRender::BT_ADD);
dst->bindTarget(); bindDeferredShader(gCineOutlineProgram);
gCineOutlineProgram.uniform2f(DEFERRED_SCREEN_RES, dst->getWidth(), dst->getHeight());
// upload outline_color / outline_params / outline_params2 from CineOutline* settings
mScreenTriangleVB->setBuffer(); mScreenTriangleVB->drawArrays(TRIANGLES,0,3);
unbindDeferredShader(gCineOutlineProgram); dst->flush(); gGL.setSceneBlendType(BT_ALPHA);
```
**Insertion (both call the same fn):**
- HDR: `pipeline.cpp:15713` — after `generateExposure` (:15706), immediately BEFORE `generateBloomHDR(&mRT->screen)`
  (:15714). Rationale: line is HDR-bright (intensity default 8 ≫ threshold 1) so blooms with no new plumbing;
  after exposure so it can't skew auto-exposure; before `colorCorrect` (:15745) so the tonemapper rolls the core
  off filmically. Alpha rides `RenderBloomAlphaGlowBoost` like prim glow.
- Legacy: `pipeline.cpp:15752` — immediately BEFORE `generateGlow(&mRT->postPingMap)` (:15753); alpha drives
  `glowExtractF.glsl:61`; `combineGlow` composites it back.
- Phase-2 fallback: if scene-alpha proves disruptive, additively splat into `mRT->bloomMip[0]` after extraction
  (mirror `feedProjectorVolumetricBloom` :15022) — halo never touches the scene buffer (costs a 2nd draw).

**Registration:** clone `gVolumetricLightProgram` block (minus atmos/shadows): VS `postDeferredNoTCV.glsl`, FS
`cineOutlineF.glsl`, `mShaderLevel[SHADER_DEFERRED]`, soft-disable on createShader failure. `mShaderList.push_back`
(:450), extern in `llviewershadermgr.h` (:224). New reserved uniforms `outline_color`/`outline_params`/
`outline_params2` in `llshadermgr.{h,cpp}` (enum order = push order). class1 only (no shadows/atmos needed).

## 4. Controls (settings.xml, CineLightRig* style)
| Setting | Type | Default | Meaning |
|---|---|---|---|
| CineOutlineEnabled | Bool | 0 | master gate (pass fully skipped off) |
| CineOutlineColor | Color3 | (1.0,0.04,0.04) | line color, linear RGB (neon red) |
| CineOutlineIntensity | F32 | 8.0 | HDR mult (≥ bloom threshold 1.0 to bloom); clamp [0,64] |
| CineOutlineThickness | F32 | 1.5 | sample spacing px; clamp [0.5,3.0] P1 |
| CineOutlineDepthSensitivity | F32 | 1.0 | scales depth gradient; 0 disables depth edges |
| CineOutlineNormalSensitivity | F32 | 1.0 | scales normal gradient; 0 disables normal edges |
| CineOutlineGlow | F32 | 1.0 | alpha glow feed [0,1] (0 = crisp, no bloom boost) |
| CineOutlineMaxDistance | F32 | 0.0 | P2: meters; 0 = whole scene; else near-subject only |
| CineOutlinePulseAmount / Speed | F32 | 0.0 / 1.0 | P2: brightness pulse on presentation time |

Subject-only: default whole-scene, which in a dark empty set IS subject-only. `CineOutlineMaxDistance` = cheap
camera-relative mask. True avatar mask has no free path (HAS_ATMOS shared with prims → needs a new gbuffer flag
bit; P2 investigation, encoding-budget risk). UI: new "Outline" section in Director floater, modeled on
`panel_cine_light_rig.xml`.

## 5. Portability / perf / composition
- Cost: one fullscreen pass, 18 point fetches, no dependent reads, no extra RTs; ≤ SMAA edge pass, ≪ glow blur
  chain / DoF. Disabled = zero (early return).
- GLSL: core helpers only (`texture`,`smoothstep`); no derivatives/texelFetch; GL 3.x class1 floor.
- Determinism: pure fn of gbuffer + settings (+ presentation-time pulse, replay-stable); skipped for cube
  snapshots (:15675) and `gSnapshotNoPost`.
- Composition: additive line over the rig-lit scene; rig blue/red rim lights live under it and both feed the
  same bloom pyramid → blend in bloom space, no fight. Near-black body = set/rig, not this pass.
- Interactions: composited pre-AA so FXAA/SMAA antialias the 1-2px line (good); CAS may ring (expose via
  RenderCASSharpness); `no_post` forces bloom threshold 99999 → line renders but won't bloom (acceptable).

## 6. Phased plan (Codex)
**Phase 1 (one sitting):** `cineOutlineF.glsl` (fixed Sobel, hard-coded thresholds, `outline_color`+`outline_params`);
reserved uniforms; program registration; `renderCineOutline` (declare in `pipeline.h` near `renderVolumetric`);
insert :15713 (HDR) + :15752 (legacy); settings `CineOutlineEnabled/Color/Intensity/Glow`. Acceptance: dark
region + avatar → full-body red contour + internal arm/torso edge, glowing; toggling off leaves other passes
byte-identical.
**Phase 2:** thickness + both sensitivities → `outline_params`; `CineOutlineMaxDistance` depth mask; pulse
(presentation time, CPU-precomputed phase); Director UI panel; optional bloomMip[0] alt feed; optional
"silhouette darken" multiplicative pre-draw; avatar-flag investigation.
**Runtime unknowns (P1 exit):** scene-buffer alpha semantics vs DoF/motion-blur/blit (drop alpha feed in HDR if
disruptive — intensity>threshold blooms alone); grazing-angle floor ringing (tune facing clamp + t0/t1);
octahedral decodeNormal ×9 cost (use `getNormRaw` if hot); FXAA softening at thin widths; legacy glow halo
quality at RenderGlowResolutionPow.
