# BDMerge — Per-Projector Volumetric Light Cones (Visible Spotlight Shafts)

Implementation brief. Net-new rendering feature (NOT a BD port). Extends the
G3.2 sun-only volumetric system (`405baf77bf`) to raymarch each shadow-casting
projector's frustum, producing visible, colored/gobo light shafts from set
lights. Scoped 2026-07-11 (Opus-tier). Proposed gate: `BDMergeProjectorVolumetrics`
(default off).

**Feasibility verdict: feasible, low-risk, well-anchored.** All prerequisites
are met. The N-projector shadow machinery (NSpot, `2352f924e0`) already delivers
per-slot shadow maps + matrices to any shadow-consuming program, and
`bindDeferredShader` already hands the volumetric program the full shadow set.
This is **C++ + a new shader** (not shader-only): it needs a new render loop, a
new program registration, and a side-effect-free variant of `setupSpotLight`.

---

## 0. Prerequisite audit (verified against current source, not the log)

| Prereq | Status | Evidence |
|--------|--------|----------|
| Per-slot spot shadow maps `mSpotShadow[0..N-1]` | ✅ met | `pipeline.h:730-732` `MAX_SPOT_SHADOWS=6`; alloc `pipeline.cpp:1097-1146` |
| Per-slot shadow matrices `shadow_matrix[4+idx]` | ✅ met | `mSunShadowMatrix[MAX_SHADOW_MATS=10]` `pipeline.h:731,782`; uploaded `pipeline.cpp:9898-9908` |
| `sampleSpotShadow(pos,norm,index,screen)` indexed dispatch | ✅ met | `shadowUtil.glsl:202-267` (dispatches shadowMap4-9 / shadow_matrix[4..9]) |
| Shadow maps bound to any shadow program | ✅ met | `bindShadowMaps` `pipeline.cpp:9753-9780` binds shadowMap4..(4+N) |
| `setupSpotLight` proj geometry + cookie | ✅ met | `pipeline.cpp:10683-10827` |
| Spot color / falloff / center / size uniforms | ✅ met | `pipeline.cpp:10393-10399` (spot loop) |
| Projector helper GLSL (`clipProjectedLightVars`, `getProjectedLightDiffuseColor`) | ✅ met | `class1/deferred/deferredUtil.glsl` (linked via `isDeferred` feature) |
| Reserved uniforms (`proj_mat`,`proj_p`,`proj_n`,`proj_shadow_idx`,`projectionMap`,`godray_*`) | ✅ met | `llshadermgr.cpp:1264-1271,1443,1679-1680` |

**No prerequisite is unmet.** One reuse hazard (not a missing prereq) is flagged
as Risk R1: `setupSpotLight` has a per-frame side effect that must not be
re-triggered from the finalize stage.

---

## 1. Existing G3.2 sun path (anchor points in current source)

- **`LLPipeline::renderVolumetric(src,dst)`** — `pipeline.cpp:9163-9192`.
  Binds `gVolumetricLightProgram` via `bindDeferredShader` (line 9172), binds
  `src` as `diffuseRect` (9173), uploads `screen_res` / `godray_res` /
  `godray_multiplier` / `falloff_multiplier` (9175-9178), draws one fullscreen
  triangle (`mScreenTriangleVB`, 9180-9181). Ping-pongs `src→dst` (fix-forward
  vs BD's read/write feedback). `else`-branch `copyRenderTarget` (9190) keeps the
  chain coherent when disabled.
- **Chain placement** — `pipeline.cpp:9522-9532`, inside `renderFinalize`.
  `sourceBuffer=&mRT->postPingMap`, `targetBuffer=&mRT->postPongMap`; the sun
  block runs **after** `colorCorrect` (9511) and legacy `generateGlow` (9519),
  **before** FXAA/SMAA (9534-9544) and CAS (9546). HDR caveat: legacy glow
  combine only exists in the non-HDR path; ordering constraint is moot in HDR
  (both paths run the pass).
- **Guards** — `RenderVolumetricLighting && RenderShadowDetail>0 &&
  !gCubeSnapshot && gVolumetricLightProgram.isComplete()` (9165, 9528).
- **Shaders** — `class3/deferred/volumetricLightF.glsl` (real raymarch, 114
  lines: loop `godray_res-1→1` at line 85, `nonpcfShadowAtPos` at 88, additive
  `diff.rgb += … * sunlight_color` at 111). `class1/deferred/volumetricLightF.glsl`
  = 44-line passthrough stub for shader levels below 3.
- **Sun helpers** — `class1/deferred/shadowUtil.glsl:272-321`:
  `nonpcfShadow` (cheap single-tap, 272-287) and `nonpcfShadowAtPos`
  (cascade select, 289-321). Both gated `#if defined(SUN_SHADOW)`.
- **Program registration** — `llviewershadermgr.cpp:2931-2951`: features
  `isDeferred/calculatesAtmospherics/hasAtmospherics/hasShadows`; permutation
  `SUN_SHADOW=1` (2938) + optional `GODRAYS_FADE` (2939-2942); shader files
  `postDeferredNoTCV.glsl` (VS) + `deferred/volumetricLightF.glsl` (FS);
  `mShaderLevel = mShaderLevel[SHADER_DEFERRED]`. Global `gVolumetricLightProgram`
  `pipeline.cpp:219`; in `mShaderList` `pipeline.cpp:398`; `unload` `1190`.
- **Reserved uniforms** — `godray_res`/`godray_multiplier`
  `llshadermgr.cpp:1679-1680`. (`falloff_multiplier` is set through the same
  block — confirm the enum ordinal at implement time.)

## 2. Projector machinery (anchor points)

- **`setupSpotLight(shader,drawablep)`** — `pipeline.cpp:10683-10827`. Three
  logical parts:
  1. **Geometry math** (10685-10751): builds `screen_to_light` and uploads
     `PROJECTOR_MATRIX` (proj_mat, 10745), `PROJECTOR_NEAR` (10746),
     `PROJECTOR_P` (proj_p / p1, 10747), `PROJECTOR_N` (proj_n, 10748),
     `PROJECTOR_ORIGIN` (10749), `PROJECTOR_RANGE` (10750), `PROJECTOR_AMBIANCE`
     (10751).
  2. **Shadow-slot lookup + priority reshuffle** (10752-10802): finds this
     drawable's slot `s_idx` in `mShadowSpotLight[i]` (10754-10760), uploads
     `PROJECTOR_SHADOW_INDEX` (10762) and `PROJECTOR_SHADOW_FADE`
     (10766/10770), then — **only if `!gCubeSnapshot`** — mutates
     `mTargetShadowSpotLight[]` by priority (10776-10802). **This mutation is
     the reuse hazard (R1).**
  3. **Cookie bind** (10804-10825): `volume->getLightTexture()` (fallback white)
     bound to `DEFERRED_PROJECTION` (projectionMap), uploads `PROJECTOR_FOCUS`
     /`PROJECTOR_LOD`/`PROJECTOR_AMBIENT_LOD`.
- **Spot render loop (deferred lighting)** — `pipeline.cpp:10369-10408`
  (near) and 10449-10479 (fullscreen). Per projector: `setupSpotLight` then
  `LIGHT_CENTER` (10396), `LIGHT_SIZE` (10397), `DIFFUSE_COLOR` = linear light
  color × `light_scale` (10394,10398), `LIGHT_FALLOFF` = `getLightFalloff(...)`
  (10399), `CLASSIC_MODE` (10400). Projectors gathered into `spot_lights` /
  `fullscreen_spot_lights` at 10332 / 10351.
- **Slot state (persist across the frame into finalize)** — `pipeline.h:787-789`:
  `mShadowSpotLight[i]` (drawable occupying shadow slot i), `mSpotLightFade[i]`,
  `mTargetShadowSpotLight[i]`. `mSunShadowMatrix[4+i]` (`pipeline.h:782`, built
  `pipeline.cpp:12434`) is that projector's shadow matrix. `getSpotShadowTarget(i)`
  `pipeline.cpp:11478-11481`. Active count = `bdmergeMaxSpotShadows()`
  `pipeline.cpp:426-430` (clamp 2..6, setting `BDMergeMaxSpotShadows` default 2).
- **`sampleSpotShadow`** `shadowUtil.glsl:202-267` — takes an explicit `index`,
  reads `shadow_matrix[4+index]` + `shadowMap(4+index)`, gated
  `#if defined(SPOT_SHADOW)`.
- **spotLightF consumer** — `class3/deferred/spotLightF.glsl:119-134`: when
  `proj_shadow_idx>=0`, `sampleSpotShadow(pos, spot_norm, proj_shadow_idx, tc)`.
  Frustum clip via `clipProjectedLightVars` (112) and `proj_tc.xy∈[0,1]` tests
  (174-175, 206-210). Cookie via `getProjectedLightDiffuseColor(l_dist, proj_tc.xy)`
  (186, 219).

### What `bindDeferredShader` already gives a volumetric program for free
`bindDeferredShader` (`pipeline.cpp:9798-`) → `bindShadowMaps` (9894) binds
shadowMap0-3 **and** shadowMap4..(4+N) (9768-9779) for **any** uniform the
program declares; uploads all 10 `shadow_matrix` (9908); uploads `shadow_res`
(9982), `proj_shadow_res` (9983), `spot_shadow_bias`/`offset` (9977-9978),
`shadow_clip` (9942), `inv_proj`, `screen_res`, sun/moon dirs. **Consequence:**
once the new program declares `shadowMap4-9` (i.e. compiles with `SPOT_SHADOW=1`),
the entire per-slot shadow set is delivered with **zero** extra C++ in the bind
path. Only the *per-light* values (proj_mat/p/n, cookie, color, falloff, center,
shadow index) are not in `bindDeferredShader` — those come from the per-slot
`setupSpotLight`-style upload.

---

## 3. Architecture ruling

**Ruling: a new dedicated program + a per-slot additive multi-pass loop.** Not a
single-pass uniform-array design, and not an in-place extension of the existing
sun draw.

- **Why multi-pass, not shader-side arrays:** each projector has an *arbitrary*
  cookie texture (`getLightTexture()`). Packing N cookies means N samplers or a
  texture array you cannot build from unrelated textures. A per-slot loop binds
  exactly one cookie at a time (`projectionMap`), reuses `setupSpotLight`'s
  geometry upload verbatim, and keeps the sampler count minimal (Risk R6 = none).
- **Why a separate program, not the sun `volumetricLightF`:** the projector march
  clips to a frustum and samples a cookie — a materially different shader. Keep
  `gVolumetricLightProgram` untouched (the sun feature stays independently
  revertable).
- **Compositing: additive, in place.** Render with `glBlendFunc(GL_ONE,GL_ONE)`
  directly onto the current `sourceBuffer` (which already holds scene + sun
  godrays). The projector shader outputs **only** the shaft contribution
  (`frag_color = shaft`), so it never samples the target it writes → **no
  read/write feedback** (sidesteps the exact BD bug G3.2 fixed) and needs **no
  extra ping-pong target**. It still reads `depthMap` (separate texture) to clip
  the march at the first opaque surface.
- **Independent gate.** The projector pass must run even when the sun godray gate
  is off, so it is its own guarded block in `renderFinalize`, not nested in the
  sun `if`.

### Frustum clip / cookie in the march (shader logic)
Per raymarch sample `spos` (view space, same `farpos` walk as the sun shader,
`volumetricLightF.glsl:78-95`):
1. `vec4 ptc = proj_mat * vec4(spos,1.0); ptc /= ptc.w;`
2. In-cone test: `ptc.z>0.0 && all(greaterThan(ptc.xy,vec2(0))) &&
   all(lessThan(ptc.xy,vec2(1)))` — matches spotLightF's convention
   (`spotLightF.glsl:206-210`). Outside → sample contributes 0.
3. Shadow: `sampleSpotShadow(spos, /*norm*/vec3(0), proj_shadow_idx, tc)` — the
   march has no surface normal, pass 0 (the `norm*offset` bias term is
   negligible for airborne samples; document as an accepted approximation).
4. Cookie color: `getProjectedLightDiffuseColor(l_dist, ptc.xy)` (gobo/colored).
5. Distance falloff: `calcLegacyDistanceAttenuation(dist_from_center, falloff)`
   using uploaded `LIGHT_CENTER`/`LIGHT_FALLOFF`.
6. Accumulate `contrib += shadow * cookie * dist_atten * color`; scale by
   `1/godray_res` and the projector multiplier as the sun shader does.

---

## 4. Step-by-step implementation plan

**C++ — new program registration** (`llviewershadermgr.cpp`, mirror 2931-2951):
1. Declare `LLGLSLShader gDeferredProjectorVolumetricProgram;` (`pipeline.cpp`
   near 219), add to `mShaderList` (near 398), `unload()` (near 1190),
   `pipeline.h` extern.
2. Register: same features as G3.2 **plus** permutation `SPOT_SHADOW=1` (so
   shadowUtil compiles the spot branch and the program declares shadowMap4-9).
   VS `postDeferredNoTCV.glsl`; FS `deferred/projectorVolumetricF.glsl`.
   `mShaderLevel = mShaderLevel[SHADER_DEFERRED]`. Guard behind the same shader
   level ≥3 gating as G3.2 (`isComplete()` check does this at runtime).

**Shaders — new files:**
3. `class3/deferred/projectorVolumetricF.glsl`: adapt `volumetricLightF.glsl`.
   Add uniforms `proj_mat`, `proj_p`, `proj_n`, `proj_range`, `proj_shadow_idx`,
   `projectionMap` (cookie), `color`, `falloff`, `center` (LIGHT_CENTER),
   `godray_res`, and the projector multiplier. Forward-declare
   `sampleSpotShadow`, `getProjectedLightDiffuseColor`,
   `calcLegacyDistanceAttenuation`, `getPosition`. Implement the §3 march.
   Output **only** the accumulated shaft (`frag_color = vec4(shaft,0.0)`).
4. `class1/deferred/projectorVolumetricF.glsl`: passthrough/no-op stub
   (`frag_color = vec4(0.0)`; additive-safe) for shader levels <3.

**C++ — the pass:**
5. Add `LLPipeline::setupSpotLightVolumetric(shader, drawablep, slot)` — a
   **side-effect-free** trim of `setupSpotLight`: geometry upload (copy
   10685-10751) + cookie bind (copy 10804-10825), **omitting** the
   `mTargetShadowSpotLight` priority block (10776-10802). Set
   `PROJECTOR_SHADOW_INDEX = slot` directly (we already know the slot). This is
   the single most important C++ item — see R1.
6. Add `LLPipeline::renderProjectorVolumetric(LLRenderTarget* target)`:
   ```
   if (!BDMergeProjectorVolumetrics || RenderShadowDetail==0 || gCubeSnapshot
       || !gDeferredProjectorVolumetricProgram.isComplete()) return;
   target->bindTarget();
   LLGLEnable blend(GL_BLEND);
   gGL.setSceneBlendType(...) / glBlendFunc(GL_ONE, GL_ONE);  // additive
   gGL.setColorMask(true,false);
   bindDeferredShader(gDeferredProjectorVolumetricProgram);   // binds shadow set
   // upload godray_res / multiplier from LLCachedControl
   for (U32 i=0; i<bdmergeMaxSpotShadows(); ++i) {
       LLDrawable* d = mShadowSpotLight[i];
       if (d==NULL || getSpotShadowTarget(i)->getWidth()==0) continue;
       LLVOVolume* v = d->getVOVolume(); if (!v) continue;
       setupSpotLightVolumetric(gDeferredProjectorVolumetricProgram, d, i);
       // LIGHT_CENTER, LIGHT_SIZE, DIFFUSE_COLOR (linear*light_scale),
       // LIGHT_FALLOFF  — copy 10394-10399
       mScreenTriangleVB->setBuffer();
       mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
   }
   unbindDeferredShader(...); gGL.setColorMask(true,true); target->flush();
   ```
7. Call site in `renderFinalize`, immediately after the sun block
   (`pipeline.cpp:9532`), as its own guarded block, additively onto the current
   `sourceBuffer` (no swap): `renderProjectorVolumetric(sourceBuffer);`

**Settings + UI:**
8. `settings.xml`: mirror 10367-10420. `BDMergeProjectorVolumetrics` (Bool, 0),
   `BDMergeProjectorVolumetricsResolution` (U32, default **24** — lower than the
   sun's 32 because cost is ×N), `BDMergeProjectorVolumetricsMultiplier`
   (F32, 1.0). Per-frame reads via `LLCachedControl` (Conventions §7).
9. `floater_lightbox_settings.xml`: add to the Godrays panel after line 2939
   (before the tip text at 2940) — a checkbox `control_name="BDMergeProjectorVolumetrics"`
   plus Brightness/Quality sliders `enabled_control="BDMergeProjectorVolumetrics"`,
   copying the widget style of 2876-2915.
10. Reserved uniforms: `proj_*`, `projectionMap`, `godray_*` already exist. Add
    a reserved-uniform string only if you introduce a *new* name (e.g. a distinct
    projector multiplier) — otherwise reuse `godray_multiplier`.

---

## 5. Gate / settings design

| Setting | Type | Default | Notes |
|---------|------|---------|-------|
| `BDMergeProjectorVolumetrics` | Bool | **0 (off)** | Master gate (Conventions default-off). Independent of `RenderVolumetricLighting`. |
| `BDMergeProjectorVolumetricsResolution` | U32 | 24 | Raymarch samples per cone. Slider 8-64. Cost is ×N — keep below the sun default. |
| `BDMergeProjectorVolumetricsMultiplier` | F32 | 1.0 | Shaft brightness. Slider 0.01-30 like G3.2. |

- Effective gate is `BDMergeProjectorVolumetrics && RenderShadowDetail>0 &&
  shaderLevel≥3 && program complete` — matches G3.2, and honestly gates on the
  same shadow requirement (shafts come from the spot shadow maps).
- No compile-time `LL_BDMERGE_*` define needed (no shader-tree restructuring; the
  new program lives alongside the sun one). Runtime gate suffices.
- UI: Godrays tab, under the existing G3.2 controls (scope statement).

---

## 6. Performance model

Cost ≈ **N × R × fullscreen-fragment work**, where N =
`min(active shadowed projectors, bdmergeMaxSpotShadows())` (≤6) and R =
`BDMergeProjectorVolumetricsResolution`. Each of the N additive passes is one
fullscreen triangle marching R samples; per sample: 1 `proj_mat` transform, an
in-cone branch, one `sampleSpotShadow` (5-tap PCF via `pcfSpotShadow`), one
cookie fetch, one attenuation. Compared to the sun pass (1 × 32 samples, 1-tap
`nonpcfShadow`), a 4-projector scene at R=24 is ≈ `4×24×5 / (1×32×1)` ≈ **15×**
the sun godray cost in shadow taps — non-trivial but bounded, and the target is a
5090. Throttles:
- **N** is naturally capped by `BDMergeMaxSpotShadows` (2 stock, 4-6 for shots).
- **R** slider (default 24) is the primary lever; scales linearly.
- Additive passes over empty frustum regions still pay per-fragment; a future
  optimization (out of scope) is a scissor/bounding-quad per cone instead of a
  fullscreen triangle. Note as a follow-up.
- Runs once per frame in `renderFinalize` (not per shadow-map, not per eye except
  where the whole finalize repeats).

Resolution-aware autoscale (A1.2) does **not** cover this pass (it never covered
the sun godrays either — see A1.2 log row); high-res snapshots need manual R
reconfiguration, same caveat as G3.2.

---

## 7. Ranked risks + mitigations

- **R1 — `setupSpotLight` priority side effect (highest).**
  `setupSpotLight` (10776-10802) mutates `mTargetShadowSpotLight[]` when
  `!gCubeSnapshot`. Calling it again from `renderFinalize` (after lighting) would
  double-apply the priority swap and corrupt next-frame shadow-slot assignment.
  **Mitigation:** implement `setupSpotLightVolumetric()` (plan step 5) that copies
  only the geometry + cookie upload and omits the priority block. Do **not** call
  `setupSpotLight` directly.
- **R2 — HDR blow-out from additive N cones.** Shafts add pre-tonemap (before
  `colorCorrect`… actually after — see note); N overlapping cones can saturate.
  **Mitigation:** per-projector multiplier + clamp the accumulated contribution
  (`min`/`clamp` in shader), start multiplier at 1.0, expose the slider.
  *Placement note:* the sun pass sits **after** `colorCorrect` (9511) in this
  tree, so shafts are added in display space, not linear HDR — verify the
  projector shaft brightness reads correctly at that stage and matches the sun
  godrays' look; if linear accumulation is wanted, that is a larger reorder and
  out of scope.
- **R3 — proj_mat convention mismatch.** The `trans` half-scale and `ptc.z>0`
  test must match spotLightF exactly or the cone projects inverted/clipped wrong.
  **Mitigation:** lift the clip test verbatim from `spotLightF.glsl:206-210`;
  validate against a single projector first.
- **R4 — only shadowed projectors get shafts (scope limit, not a bug).** Slots
  beyond `bdmergeMaxSpotShadows()` (non-shadow-casting projectors) produce no
  volumetrics. **Mitigation:** document it; raising `BDMergeMaxSpotShadows`
  (up to 6) promotes more projectors to shadowed = shafted. Acceptable for
  machinima (you choose which set lights cast).
- **R5 — banding at low R × missing dither.** The sun shader jitters with
  `rand(tc)` (`volumetricLightF.glsl:77,87`); the projector march must carry the
  same per-pixel offset or low-R shafts stair-step. **Mitigation:** reuse the
  `roffset` jitter; AA pass (after) smooths residual banding.
- **R6 — sampler budget: not a concern.** New program binds `depthMap` +
  shadowMap4-9 (6) + `projectionMap` + a handful from `bindDeferredShader` ≈
  ~10/32 units (NSpot established spot shaders use 7/32). No overflow.
- **R7 — shader-level / feature-table dependency.** Like G3.2, class3 real shader
  needs deferred shader level 3; class1 stub is a no-op. **Mitigation:** the
  `isComplete()` runtime guard already handles absence; the additive stub is
  harmless at low levels.
- **R8 — `mShadowSpotLight[i]` staleness at finalize.** It is populated during
  `generateSunShadow` earlier in the same frame and is valid at `renderFinalize`.
  Low risk; assert `notNull` + `getVOVolume()` before use.

---

## 8. Estimate

- **Shader-only? No.** Requires C++ (new program globals + registration, a new
  render loop, a side-effect-free `setupSpotLightVolumetric`, settings) **plus**
  two new GLSL files and one UI panel edit. Needs an **executable rebuild**
  (unlike NSpot, which was one-shader-no-C++).
- **Session count: 2-3.**
  - *Session 1:* program registration + `projectorVolumetricF.glsl` +
    `setupSpotLightVolumetric` + single-projector additive pass working;
    settings + UI wired. Verify one cone in-world.
  - *Session 2:* multi-slot loop, cookie/color/falloff accumulation, jitter,
    clamp/multiplier tuning, HDR-stage brightness check (R2), banding pass.
  - *Session 3 (contingent):* in-world acceptance across N=2/4/6, perf capture,
    final tuning. Fold into S2 if S1/S2 go clean.
- **One revertable commit** `[BDMerge <id>] Per-projector volumetric light cones`;
  gate default off keeps it inert until toggled.

---

## 9. Deliberate scope cuts (rulings)

- **Shadow-casting projectors only.** No shafts from non-shadowed spots (R4).
- **Fullscreen triangle per cone**, not a scissored bounding quad — simpler,
  accepts the empty-region fragment cost; bounding-quad optimization is a
  follow-up.
- **Reuse G3.2's display-stage placement** (after `colorCorrect`). No move to
  linear-HDR accumulation (larger reorder, out of scope; R2 note).
- **No A1.2 autoscale integration** — parity with the sun godrays; manual R for
  high-res snapshots.
- **Cookie sampled as `getProjectedLightDiffuseColor`** (light color × gobo). No
  separate per-shaft color grading beyond the existing light color × multiplier.
```
