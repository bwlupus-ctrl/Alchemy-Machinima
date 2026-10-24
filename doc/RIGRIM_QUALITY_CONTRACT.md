# Rig Rim Quality — shader/C++ contract (WP0/WP1, Fable, 2026-10-09)

**2026-10-23 alpha-fringe correction (supersedes the historical C-pass placement
and GL-state pseudocode below):** beauty C1-C3 now run through
`renderRigRimWrap(false)` at the first `POOL_ALPHA_PRE_WATER` boundary in
`renderGeomPostDeferred`, after forward opaque pools and before transparency.
Blended hair cannot feed white colour into the background blur, and its normal
coverage blend occludes opaque wrap. Use live `gGLProjection` there; the end-of-scene
snapshot has not updated `gGLLastProjection` yet. The helper preserves the parent
target stack, routes only attachment 0, and saves/restores that index's blend,
equations and colour mask without nesting the pool loop's sidecar guard.
`renderRigRimWrap(true)` retains Wrap-only debug in `renderFinalize`; other debug
views retain their placement. Rim OFF returns before allocation or GL work.
`scripts/code_tools/rim_alpha_gl_check.py` exercises production shaders, alpha
ramps and the production C++ state transaction on real WGL. This fixes a spatial
compositing defect; Steady has no temporal history.

**2026-10-23 wrap-strength correction (supersedes the historical raw-additive
math below):** the RGB increment is `max(bg - fg, 0) * k`, where
`k = min(0.6, a*a * sqrt(clamp(Master, 0, 4)) * prox * graze * gate)`.
`a` is Amount clamped to 0..1. Background is blurred, clamped per channel to
`CineRigRimWrapClamp` (now 0.5..1), then desaturated. Each channel approaches a
brighter background; a brighter foreground channel stays unchanged. Master is
the overall rim level; its square root retains useful wrap at low Master.

`diffuseRect` / `DEFERRED_DIFFUSE` reads the exact unblurred pre-wrap foreground
from a dedicated full-resolution RGBA16F `rigRimWrapScene` snapshot, mip 0,
point sampled. Fixed-function blending cannot clamp `bg - destination` per
channel, and sampling the active screen texture is undefined; this portable
implementation therefore needs one full-resolution colour blit. It costs 8
bytes/pixel of scratch memory (about 63 MiB at 4K) plus copy bandwidth when wrap
runs. Allocation failure skips wrap. The copy reads attachment 0 explicitly;
the helper also restores read-buffer routing. BT_ADD, RGB-only writes, the glow
channel, and the pre-alpha beauty placement remain as above. No work occurs
with Wrap OFF or Amount zero.

Director and Lightbox Amount use 0..1 in 0.01 steps; default 0.5 becomes 0.25.
Old settings, scenes and custom presets have no amount-version marker: the
setting-layer callback preserves numeric values in 0..1, saturates values above
1, floors negatives to zero, and replaces non-finite values with zero. It runs
at startup and on every subsequent setting write, so repeated loads are stable.
The new squared curve applies to all values; legacy raw-glow brightness is
intentionally reduced. Legacy Clamp values above 1 saturate at upload and in the
shader. Clamp stays a Debug setting, without another Director row.

All 40 built-ins retain their widths, blur, saturation and other rim settings;
Amount is retuned to roughly half, with every Clamp set to 1. The complete
old/new values are recorded in the table at the end of this contract.

Design: `C:\Users\xianw\cxs\rim_quality_design.md`. This document is the complete
interface between the shaders (DONE, listed in §1) and the C++ package (Codex,
WP2). A C++ implementer must not need to read the shaders to wire them.

Phase 1 scope (this batch): items 1–4 (smooth normal, anti-shimmer, silhouette
band, actors-only, light wrap) on the **deferred** light programs, item 1 only
(smooth normal + anti-shimmer) on **forward alpha**. Phase 2 (silhouette band /
actors-only on forward alpha) is deferred; the forward pixel already carries
neutral `prox` / `subject` so Phase 2 is a producer-side change only.

Companion: `doc/LENS_LOOKS_CONTRACT.md` ships in the same batch and appends its
reserved uniforms **after** the block in §2 — the combined order is fixed and is
repeated in both documents.

---

## 1. Shader files (written)

| File | Change |
|---|---|
| `class1/deferred/deferredUtil.glsl` | `rig_rim_quality`, `rig_rim_sil`, `rigRimAuxMap`, `rigRimSilMap`; `struct RigRimPixel` (guarded); `rigRimNeutralPixel`, `rigRimForwardPixel`, `rigRimOctEncode/Decode`, `rigRimSilBandWidth`, `rigRimSilProx`, `rigRimLoadPixel`, `rigRimTermEx`; `RigRimPixel` overloads of `pbrCalcPointLightOrSpotLightRimSplit / RimKeep / RimKeepRef` (old signatures forward a neutral pixel). `rigRimTerm()` is kept verbatim. |
| `class3/deferred/spotLightF.glsl`, `pointLightF.glsl`, `multiPointLightF.glsl` | `RigRimPixel rim_px` loaded under `#ifdef RIG_RIM_AUX` (only when `rigRimMode() != 0`); every `rigRimTerm(...)` → `rigRimTermEx(..., rim_px)`. |
| `class2/deferred/pbralphaF.glsl`, `class2/deferred/sharedActorFxPbrF.glsl` | local pixel via `rigRimForwardPixel` (vertex normal + `fwidth`), passed through the LIGHT_LOOP macros (all three ALR variants). |
| `class2/deferred/alphaF.glsl`, `class3/deferred/materialF.glsl` | `calcPointLightOrSpotLight(..., vec4 rim, RigRimPixel rim_px [, ALR tail])`; local pixel before the LIGHT_LOOP. |
| NEW `class1/deferred/rigRimAuxF.glsl` | pass A: aux map producer |
| NEW `class1/deferred/rigRimSilSeedF.glsl` | pass B1: silhouette seeds (half-res); reads `rigRimAuxMap` (pass A is a prerequisite) |
| NEW `class1/deferred/rigRimSilDtF.glsl` | passes B2/B3: separable windowed EDT (permutation `RIG_RIM_DT_VERTICAL`) |
| NEW `class1/deferred/rigRimWrapBlurF.glsl` | passes C1/C2/C3a: quarter-res 4×4 box downsample (permutation `RIG_RIM_WRAP_DOWNSAMPLE`) + separable Gaussian |
| NEW `class1/deferred/rigRimWrapF.glsl` | pass C3: additive light-wrap composite |
| NEW `class1/deferred/rigRimDebugF.glsl` | pass D: developer views 2/3/4 |

All new passes use the existing screen-triangle vertex shader
`deferred/postDeferredNoTCV.glsl` (`vary_fragcoord`); no new vertex shader.

**Compile-unit facts the C++ must respect** (verified in `llshadermgr.cpp`):
utility objects (`deferredUtil.glsl`) are compiled once and cached by filename
(`mFragmentShaderObjects[filename]`), so per-program permutations never reach
them — only `sGlobalDefines` do. That is why the aux loader lives in
`deferredUtil.glsl` unconditionally (its two samplers are *inactive* in every
program that does not call `rigRimLoadPixel`, i.e. everything except the light
programs) and why `RIG_RIM_AUX` only gates the call sites in the three light
main files. `struct RigRimPixel` is defined identically in each compile unit
that passes it across the unit boundary, guarded by `RIG_RIM_PIXEL_DEFINED`
(same mechanism as the preamble-injected `GBufferInfo`). Do **not** also inject
it from the preamble unless you also define `RIG_RIM_PIXEL_DEFINED` there.

---

## 2. Reserved uniforms — `llshadermgr.h` / `llshadermgr.cpp`

Append in **this exact order** after `RENDER_DEBUG_LIGHTING_TERM` (h: before
`END_RESERVED_UNIFORMS`; cpp: after `push_back("render_debug_lighting_term")`,
before the `llassert(size == END_RESERVED_UNIFORMS)`). Count-verify: 8 rim + 6
lens = 14 new entries.

| # | enum | name string | type | used by |
|---|---|---|---|---|
| 1 | `RIG_RIM_QUALITY` | `"rig_rim_quality"` | vec4 | deferredUtil (all rim consumers) |
| 2 | `RIG_RIM_SIL` | `"rig_rim_sil"` | vec4 | deferredUtil, rigRimWrapF, rigRimDebugF |
| 3 | `RIG_RIM_AUX_PARAMS` | `"rig_rim_aux_params"` | vec4 | rigRimAuxF, rigRimSilSeedF, rigRimDebugF |
| 4 | `RIG_RIM_AUX_MAP` | `"rigRimAuxMap"` | sampler2D | deferredUtil (light programs), rigRimWrapF, rigRimDebugF |
| 5 | `RIG_RIM_SIL_MAP` | `"rigRimSilMap"` | sampler2D | deferredUtil (light programs), rigRimSilDtF (its input), rigRimWrapF, rigRimDebugF |
| 6 | `RIG_RIM_WRAP` | `"rig_rim_wrap"` | vec4 | rigRimWrapF |
| 7 | `RIG_RIM_WRAP2` | `"rig_rim_wrap2"` | vec4 | rigRimWrapBlurF (xy), rigRimWrapF (x,z,w) |
| 8 | `RIG_RIM_WRAP_SRC` | `"rigRimWrapSrc"` | sampler2D | rigRimWrapBlurF (input), rigRimWrapF (blurred scene) |
| 9–14 | Lens Looks block, see `doc/LENS_LOOKS_CONTRACT.md` §2 (`ON_LENS_ALBEDO`, `LENS_PYRAMID0..4`) | | | |

Existing reserved entries reused: `ACTOR_MASK_MAP "actorMaskMap"`,
`ACTOR_MASK_BITS "actor_mask_bits"` (rigRimAuxF), `RIG_RIM_MODE/GLOBALS/...`
(unchanged), `DEFERRED_SCREEN_RES "screen_res"` (uploaded by `bindDeferredShader`).

### Packing

```
rig_rim_quality  x Smoothness 0..1   y AntiShimmer 0..1   z Silhouette 0..1   w ActorsOnly 0/1
                 vec4(0) == bit-exact OFF (also the GL default of a never-uploaded uniform)
rig_rim_sil      x width: metres at the subject when w > 0, else half-res texels
                 y softness 0..1 (fraction of the width)
                 z near gap: relative depth tolerance, |z - z_seed| <= gap * z_seed (fades to 0 at 2*gap)
                 w half-res texels per metre at 1 m view depth (0 => x is already texels)
                   = 0.5 * deferredScreen.getHeight() * gGLProjection[5] * 0.5   (see pipeline.cpp 25228
                     px_per_m, halved for the half-res map)
rig_rim_aux_params
                 x subject source: 0 none (mask = 1 everywhere), 1 avatar tag, 2 Actor Mask
                 y seed threshold (relative tangent-plane residual; default 0.02)
                 z seed softness 0..1 (default 0.5)
                 w debug view id for rigRimDebugF (2 band, 3 smooth normal, 4 subject)
rig_rim_wrap     x perceptual amount (0..1)   y reach in half-res texels   z saturation 0..1   w background clamp (0.5..1)
rig_rim_wrap2    xy uv step per tap for the Gaussian pass being drawn, in QUARTER-res texels (review P2-2):
                   C1 (downsample permutation): unused (upload 0,0)
                   C2 horizontal: (blur_q / rigRimWrapBlur[0].getWidth(), 0)
                   C3a vertical : (0, blur_q / rigRimWrapBlur[0].getHeight())
                   blur_q = CineRigRimWrapBlur (quarter texels per tap; 1.0 ~ sigma 2 quarter texels ~ 8 full px)
                 x Master gain (0..4) in the composite program only
                 z debug 1 => the wrap composite outputs the wrap term alone (host draws it with blend OFF)
                 w subject gate 0/1 (ActorsOnly: wrap *= subject)
```

Textures:

| sampler | target | format | size | filter when bound |
|---|---|---|---|---|
| `rigRimAuxMap` | `mMainRT.rigRimAux` | `GL_RGBA16` (unorm) | deferredScreen w × h | `TFO_POINT` |
| `rigRimSilMap` | `mMainRT.rigRimSil[0]` (final), `[1]` (ping) | `GL_RGBA16F` | ⌈w/2⌉ × ⌈h/2⌉ | `TFO_POINT` (mandatory: offset/depth must not blend across seeds) |
| `rigRimWrapSrc` | `mMainRT.rigRimWrapBlur[0]` (final + C2 input), `[1]` (C3a input) | `GL_R11F_G11F_B10F` (`GL_RGBA16F` if the 11/11/10 allocation fails) | ⌈w/4⌉ × ⌈h/4⌉ | `TFO_BILINEAR` |
| `diffuseRect` | `mMainRT.rigRimWrapScene` | `GL_RGBA16F` | screen w × h | `TFO_POINT` |

Encodings: aux `xy` = octahedral smooth normal (eye space) in 0..1, `z` =
Toksvig |n̄| 0..1, `w` = subject 0/1. Sil `xy` = **signed offset to the nearest
seed in half-res texels** (fp16-exact integers, |offset| ≤ 24; a seed stores
(0,0); the sky stores (0,0) with `z = 6e4`), `z` = seed view depth (m), `w` =
Euclidean distance in half-res texels, `1e4` (=`RIG_RIM_DT_BIG`) when no seed
lies within `RIG_RIM_DT_R = 24` texels. (Changed from "absolute seed uv" after
review P2-1: fp16 uv spacing at 4K is ~1 half-res texel, which made the wrap
direction noise.) The C++ never decodes the sil map; only the shaders do.

---

## 3. Programs to register (`llviewershadermgr.cpp`)

Register next to `gCineOutlineProgram` (~line 3999). Every new program **soft-fails**
(`success = true` after a `LL_WARNS`, like Cine Outline); the call sites gate on
`isComplete()`. `clearPermutations()` **before** `addPermutation()` (CLAUDE.md
hazard).

| program | mName | features | files | permutations |
|---|---|---|---|---|
| `gRigRimAuxProgram` | "Rig Rim Aux Shader" | `isDeferred` | `deferred/postDeferredNoTCV.glsl` + `deferred/rigRimAuxF.glsl` | — |
| `gRigRimSilSeedProgram` | "Rig Rim Silhouette Seed Shader" | `isDeferred` | postDeferredNoTCV + `deferred/rigRimSilSeedF.glsl` | — |
| `gRigRimSilDtProgram[2]` | "Rig Rim Silhouette DT H / V" | none | postDeferredNoTCV + `deferred/rigRimSilDtF.glsl` | `[1]`: `RIG_RIM_DT_VERTICAL = 1` |
| `gRigRimWrapBlurProgram[2]` | "Rig Rim Wrap Downsample" / "Rig Rim Wrap Blur" | none | postDeferredNoTCV + `deferred/rigRimWrapBlurF.glsl` | `[0]`: `RIG_RIM_WRAP_DOWNSAMPLE = 1` (C1 box); `[1]`: none (C2/C3a Gaussian) |
| `gRigRimWrapProgram` | "Rig Rim Wrap Shader" | `isDeferred` | postDeferredNoTCV + `deferred/rigRimWrapF.glsl` | — |
| `gRigRimDebugProgram` | "Rig Rim Debug Shader" | `isDeferred` | postDeferredNoTCV + `deferred/rigRimDebugF.glsl` | — |

`mShaderLevel = mShaderLevel[SHADER_DEFERRED]` for all.

**Existing programs gaining a permutation** — add `addPermutation("RIG_RIM_AUX", "1")`
after their `clearPermutations()` / `add_common_permutations()`:
`gDeferredLightProgram` (pointLightF), every `gDeferredMultiLightProgram[i]`
(multiPointLightF), `gDeferredSpotLightProgram`, `gDeferredMultiSpotLightProgram`
(spotLightF), `gDeferredMultiLightActorMaskProgram`,
`gDeferredMultiSpotLightActorMaskProgram` (~line 2704–2830). Without the
permutation the file compiles to today's behaviour (neutral pixel), so the
permutation is also the kill switch.

**Texture-unit risk (design risk 1, review P2-4):** with `RIG_RIM_AUX` the light
programs activate two more samplers. **An overflow does NOT fail the link** —
`llglslshader.cpp:526` only warns and silently reads unit 0 — so the check must
be explicit: after each light program's `createShader()` test
`shader.mActiveTextureChannels > gGLManager.mNumTextureImageUnits` (and log the
count). On overflow (or a genuine link failure) rebuild that program **without**
`RIG_RIM_AUX` and set `gRigRimAuxCompiled = false` (status text "Rim quality
unavailable: shader fallback"); the whole feature then degrades to the plain rim
because `bindRigRimAux` zeroes `rig_rim_quality` when `!gRigRimAuxCompiled`.
Never let it fail the stock shader load.

---

## 4. Passes, order, insertion points (re-verified `pipeline.cpp` @ e189340efa4)

### 4.1 Pre-passes A/B — `renderDeferredLighting(ActorMaskViewSource source)`

Insert **after** the soften-shadow block's closing brace (line 25258) and
**before** `bindPrismLensTarget(*screen_target);` (line 25260). The enclosing
scope has `LLGLEnable cull(GL_CULL_FACE); LLGLEnable blend(GL_BLEND);` (25115–16),
so every pass below scopes `LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);`
exactly like the soften pass. `bindTarget()` sets the viewport; `flush()`
unbinds; the following `bindPrismLensTarget(*screen_target)` rebinds the beauty
target, so nothing else needs restoring.

Predicate `rim_quality_main`:
```
source == ActorMaskViewSource::MAIN && mRT == &mMainRT && !gCubeSnapshot &&
!sRenderingHUDs && !sImpostorRender && !sPrismLensRender &&
ALCineRigRim::isEnabled() && gRigRimAuxCompiled
```
Work selection (settings in §7): `want_aux = Smoothness>0 || AntiShimmer>0 || ActorsOnly || WrapEnabled`,
`want_sil = Silhouette>0 || WrapEnabled`. **Pass A is a prerequisite of B1**
(the seed pass reads the smooth normal from `rigRimAuxMap`, review B3), so the
block below always runs A first whenever `want_aux || want_sil`. Set `mRigRimAuxValid = mRigRimSilValid = false`
at the top of every MAIN-view `renderDeferredLighting` (not in the
`ResetActorMaskView` destructor — see §5), then:

```
if (rim_quality_main && (want_aux || want_sil) && ensureRigRimAux())
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - rig rim aux");
    LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);

    // A  (full-res) -------------------------------------------------- GPU zone "rig rim aux"
    mMainRT.rigRimAux.bindTarget();                      // no clear: every pixel is written
    bindDeferredShader(gRigRimAuxProgram);               // depthMap/normalMap/inv_proj/screen_res
    uniform4f(RIG_RIM_AUX_PARAMS, subject_source, seed_thr, seed_soft, 0)
    if (subject_source == 2) bindActorMask(gRigRimAuxProgram) /* existing helper: actorMaskMap + actor_mask_bits (union of live recipient bits) */
    mScreenTriangleVB->setBuffer(); drawArrays(TRIANGLES, 0, 3);
    unbindDeferredShader(gRigRimAuxProgram);  mMainRT.rigRimAux.flush();
    mRigRimAuxValid = true;

    if (want_sil)
    {
        // B1 seeds (half-res) ------------------------------------------ GPU zone "rig rim sil seed"
        rigRimSil[0].bindTarget(); bindDeferredShader(gRigRimSilSeedProgram);
        bindTexture(RIG_RIM_AUX_MAP, &mMainRT.rigRimAux, false, TFO_POINT);   // smooth normal for the tangent plane
        uniform4f(RIG_RIM_AUX_PARAMS, ...same...); draw; unbind; flush;
        // B2 DT horizontal: sil[0] -> sil[1] -------------------------- GPU zone "rig rim sil dt"
        rigRimSil[1].bindTarget(); gRigRimSilDtProgram[0].bind();
        bindTexture(RIG_RIM_SIL_MAP, &rigRimSil[0], false, TFO_POINT); draw; unbind; flush;
        // B3 DT vertical: sil[1] -> sil[0]
        rigRimSil[0].bindTarget(); gRigRimSilDtProgram[1].bind();
        bindTexture(RIG_RIM_SIL_MAP, &rigRimSil[1], false, TFO_POINT); draw; unbind; flush;
        mRigRimSilValid = true;                           // final result is rigRimSil[0]
    }
}
```
The aux pass runs **after** the Actor Mask of this view has been produced
(line ~25034 onward) so source 2 sees a valid mask. Subject-source resolution
(C++, once per frame):
1. `ActorsOnly && gbufferAvatarTagWanted() && tag compiled` → 1;
2. else `ActorsOnly && mActorMaskValid && ALCineLightRigManager::anyActorOnly()` → 2 with
   `actor_mask_bits` = union of the live recipient bits;
3. else → 0, and if `ActorsOnly`: **LOUD status** in the Quality card ("Actors only has no
   avatar tag and no Actor Mask — rim is world-wide") + `LL_WARNS_ONCE("RigRimQ")`, plus the
   one-click "Enable avatar tag" button (sets `RenderGBufferAvatarTag`, confirmation dialog,
   shader reload). No auto-enable.

### 4.2 Binding into the light programs — `bindDeferredShader` (line 23769)

Right after `ALCineRigRim::bindGlobals(shader);` add `bindRigRimAux(shader);`:
```
void LLPipeline::bindRigRimAux(LLGLSLShader& shader)
{
    if (shader.getUniformLocation(LLShaderMgr::RIG_RIM_QUALITY) < 0) return;   // no deferredUtil
    const bool main_view = mRT == &mMainRT && !gCubeSnapshot && !sRenderingHUDs &&
                           !sImpostorRender && !sPrismLensRender;
    const bool aux_ok = main_view && mRigRimAuxValid;
    const bool sil_ok = main_view && mRigRimSilValid;
    shader.uniform4f(RIG_RIM_QUALITY, aux_ok ? smooth : 0, aux_ok ? anti : 0,
                                      sil_ok ? sil : 0,   aux_ok && actors_only ? 1 : 0);
    shader.uniform4f(RIG_RIM_SIL, width_x, soft, gap, texels_per_m_or_0);
    if (shader.getUniformLocation(RIG_RIM_AUX_MAP) >= 0 && aux_ok) shader.bindTexture(RIG_RIM_AUX_MAP, &mMainRT.rigRimAux, false, TFO_POINT);
    if (shader.getUniformLocation(RIG_RIM_SIL_MAP) >= 0 && sil_ok) shader.bindTexture(RIG_RIM_SIL_MAP, &mMainRT.rigRimSil[0], false, TFO_POINT);
}
```
Zeroing the components is what makes probes / Prism / impostors / HUD / mirrors
take the plain path: the shaders never sample a map whose component is 0, so an
unbound sampler is never read. **Fast bind:** `bindDeferredShaderFast`'s fast
branch skips `bindGlobals`/`bindRigRimAux`; any change of `mRigRimAuxValid`,
`mRigRimSilValid` or the quality settings must invalidate the fast path (the same
mechanism `ALCineRigRim::isEnabled() -> gGL.setRigRimActive()` uses to bump the
light-state hash). `ALCineRigRim::bindGlobals` may keep returning early for mode
0 — the shaders only read `rig_rim_quality` inside `rigRimMode() != 0`.

**Forward alpha (review B2 — REQUIRED, not optional).** The forward-lit programs
(pbralphaF / alphaF / materialF / sharedActorFxPbrF) are bound through
`lldrawpoolalpha.cpp:131 ALCineRigRim::bindGlobals(*shader, true)`, never through
`bindDeferredShader`, so `bindRigRimAux` never sees them and `rig_rim_quality`
would stay at the GL default 0 — killing Phase-1 item 1 on alpha. Therefore
`ALCineRigRim::bindGlobals` itself, whenever the resolved mode is non-zero and
`forward_alpha` is true, must upload
```
shader.uniform4f(LLShaderMgr::RIG_RIM_QUALITY, smoothness, anti_shimmer, 0.f, 0.f);
```
(z/w are Phase 2 on alpha and must be 0; the forward pixel is local, no samplers
are bound and `rig_rim_sil` is not needed). When `forward_alpha` is false
`bindGlobals` leaves `rig_rim_quality` to `bindRigRimAux` (which runs right after
it in `bindDeferredShader`). `CineRigRimSmoothness` / `CineRigRimAntiShimmer`
must be part of the fast-bind invalidation (`bindDeferredShaderFast`) together
with the two validity flags: fold them into the same gain-hash / light-state
hash that `CineRigRimMasterGain` already bumps, so a slider change re-uploads on
the next bind of every fast-bound program.

Width conversion (`rig_rim_sil.x/.w`):
- Units = cm: `x = CineRigRimSilhouetteWidth * 0.01` (metres), `w = texels_per_m` (above).
- Units = px@1080p: `x = CineRigRimSilhouetteWidth * (deferredScreen.getHeight() / 1080.0) * bdmerge_snapshot_autoscale_multiplier() * 0.5` (full-res px → half-res texels), `w = 0`.
- Status "width clamped": the shader clamps the per-pixel width to `24 / (1 + soft)` texels; the C++ evaluates the same formula at the subject distance (Director subject A, else 3 m) and shows "Edge width clamped to N cm at the subject" when hit.

### 4.3 Light wrap C1–C3 — `renderFinalize`

Insert at the **start of the `if (hdr)` block** (line 22546, before
`updateNightMaskAnchor()`), i.e. right after `renderWeather(&mRT->screen);` and
before luminance metering (the wrap is scene light, metered like weather).

Guard: `hdr && mRT == &mMainRT && !gCubeSnapshot && !sPrismLensRender && !sImpostorRender && !sRenderingHUDs && WrapEnabled && Strength > 0 && mRigRimAuxValid && mRigRimSilValid && gRigRimWrapProgram.isComplete() && gRigRimWrapBlurProgram[0].isComplete() && gRigRimWrapBlurProgram[1].isComplete() && ensureRigRimWrap()`.

```
LL_PROFILE_GPU_ZONE("rig rim wrap blur");
{ LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);
  const F32 blur_q = CineRigRimWrapBlur;                      // quarter-res texels per tap, both axes (isotropic, review P2-2)
  // C1 full -> quarter[0]: 4x4 box downsample (permutation RIG_RIM_WRAP_DOWNSAMPLE)
  rigRimWrapBlur[0].bindTarget(); gRigRimWrapBlurProgram[0].bind();
  bindTexture(RIG_RIM_WRAP_SRC, &mRT->screen, false, TFO_BILINEAR);
  uniform4f(RIG_RIM_WRAP2, 0, 0, 0, 0); draw; flush; unbind;
  // C2 quarter[0] -> quarter[1]: horizontal Gaussian
  rigRimWrapBlur[1].bindTarget(); gRigRimWrapBlurProgram[1].bind();
  bindTexture(RIG_RIM_WRAP_SRC, &rigRimWrapBlur[0], false, TFO_BILINEAR);
  uniform4f(RIG_RIM_WRAP2, blur_q / rigRimWrapBlur[0].getWidth(), 0, 0, 0); draw; flush;
  // C3a quarter[1] -> quarter[0]: vertical Gaussian  (final blurred scene = rigRimWrapBlur[0])
  rigRimWrapBlur[0].bindTarget();
  bindTexture(RIG_RIM_WRAP_SRC, &rigRimWrapBlur[1], false, TFO_BILINEAR);
  uniform4f(RIG_RIM_WRAP2, 0, blur_q / rigRimWrapBlur[0].getHeight(), 0, 0); draw; flush; unbind; }
LL_PROFILE_GPU_ZONE("rig rim wrap");
{ mRT->screen.bindTarget();
  LLScopedIndexedDrawBufferGuard att0_only;   // narrow to attachment 0 (sidecar/coverage must not receive light) — same guard renderCineOutline uses
  gGL.setColorMask(true, false);              // keep the glow tag in alpha
  LLGLDepthTest depth(GL_FALSE, GL_FALSE); LLGLDisable cull(GL_CULL_FACE);
  LLGLEnable blend(GL_BLEND); gGL.setSceneBlendType(debug_wrap_only ? /*blend disabled instead*/ : LLRender::BT_ADD);
  bindDeferredShader(gRigRimWrapProgram);     // depthMap + inv_proj like renderCineOutline at this slot
  bindTexture(RIG_RIM_AUX_MAP, &mMainRT.rigRimAux, false, TFO_POINT);
  bindTexture(RIG_RIM_SIL_MAP, &mMainRT.rigRimSil[0], false, TFO_POINT);
  bindTexture(RIG_RIM_WRAP_SRC, &rigRimWrapBlur[0], false, TFO_BILINEAR);   // the C3a result
  uniform4f(RIG_RIM_SIL, ...)          // explicit: bindGlobals returns early in mode 0 and never uploads it here
  uniform4f(RIG_RIM_WRAP, strength, reach_texels, saturation, clamp);
  uniform4f(RIG_RIM_WRAP2, 0, 0, debug_wrap_only ? 1 : 0, actors_only ? 1 : 0);
  draw; unbindDeferredShader; mRT->screen.flush();
  gGL.setSceneBlendType(LLRender::BT_ALPHA); gGL.setColorMask(true, true); }
```
`blur_q = CineRigRimWrapBlur` in quarter-res texels per tap for BOTH Gaussian
passes (the old `blur_px = 2 × Blur` with mixed full/quarter units is gone — it
made the blur ~4× taller than wide).
`reach_texels = CineRigRimWrapWidth * bandwidth_texels_at_subject` (band width
from §4.2 evaluated at the subject distance; px mode: the texel width).
State to prove for review (CLAUDE.md rule 1): renderFinalize entered with
`LLGLDisable blend`, `gGL.setColorMask(true,true)`, `LLGLDepthTest depth(GL_FALSE)`;
every one of them must be back when this block ends, and the draw buffer must be
attachment 0 only while `RenderVisibleDiffuseSidecar` or the ALR parity
attachment is present.

### 4.4 Debug D — `renderFinalize`, right after C3

`CineRigRimDebugView`: 1 → drive `CineRigRimDebugRimOnly` (existing rim-only mode
2 in the light shaders); 5 → run C3 with blend **off** and `rig_rim_wrap2.z = 1`
(wrap term alone); 2/3/4 → after C3:
```
LL_PROFILE_GPU_ZONE("rig rim debug");
mRT->screen.bindTarget(); LLScopedIndexedDrawBufferGuard att0_only; LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);
bindDeferredShader(gRigRimDebugProgram); bind RIG_RIM_AUX_MAP / RIG_RIM_SIL_MAP (TFO_POINT);
uniform4f(RIG_RIM_SIL, ...); uniform4f(RIG_RIM_AUX_PARAMS, subject_source, seed_thr, seed_soft, view);
draw; unbind; flush;
```
Views 2–4 require `mRigRimAuxValid` (and `mRigRimSilValid` for 2); otherwise show
the status "debug view needs the aux pass" and draw nothing.

### 4.5 Order summary

```
renderDeferredLighting(MAIN): ... sun / soften ... -> [A aux] -> [B1 seed, B2 DT-H, B3 DT-V] -> bindPrismLensTarget -> local lights (consume via bindRigRimAux)
renderFinalize: copySSR -> renderWeather -> if(hdr){ [C1 C2 blur, C3 wrap, D debug] -> updateNightMaskAnchor -> ... luminance/exposure -> Tron -> RotoInk -> onLens -> bloom }
```

---

## 5. Validity rules

- `mRigRimAuxValid` / `mRigRimSilValid` (bools on `LLPipeline`) are set **only** by the MAIN-view passes of §4.1 and cleared (a) at the top of every MAIN-view `renderDeferredLighting`, (b) in `releaseRigRimAux()`, (c) on any resize of `mMainRT` (`resizeScreenTexture` / `allocateScreenBuffer`) and (d) on shader reload. They intentionally **survive** the `ResetActorMaskView` destructor so the wrap pass in `renderFinalize` (same frame, after `renderDeferredLighting` returns) can consume them — this is a deliberate deviation from the design's "reset in the ResetActorMaskView destructor", which would have killed the wrap pass.
- Every consumer additionally requires the main-view predicate (`mRT == &mMainRT && !gCubeSnapshot && !sRenderingHUDs && !sImpostorRender && !sPrismLensRender`). Probes, Prism, mirrors/hero probes, impostors and HUD therefore get `rig_rim_quality == vec4(0)` ⇒ the plain rim.
- Forward alpha never binds the samplers; its pixel is local (`rigRimForwardPixel`), so `Smoothness` / `AntiShimmer` apply there everywhere the rim itself applies (`CineRigRimIncludeAlpha`), and `Silhouette` / `ActorsOnly` are no-ops there in Phase 1 (neutral `prox`/`subject` = 1).
- Actor Mask consumption (source 2) happens only inside pass A, inside the Actor Mask's own view/validity window — the light shaders never touch the mask.

---

## 6. Targets (`RenderTargetPack`, next to `actorMask`)

```
LLRenderTarget rigRimAux;          // GL_RGBA16,         w x h,          no depth
LLRenderTarget rigRimSil[2];       // GL_RGBA16F,        ceil(w/2) x ceil(h/2), no depth
LLRenderTarget rigRimWrapBlur[2];  // GL_R11F_G11F_B10F, ceil(w/4) x ceil(h/4), no depth (fallback GL_RGBA16F)
```
`bool ensureRigRimAux()` (aux + sil, main RT only; re-allocate when
`deferredScreen` size differs; `LL_WARNS_ONCE` + false on failure),
`bool ensureRigRimWrap()`, `void releaseRigRimAux()` (all five; also clears the
valid flags). Release on the off transition: `!CineRigRimEnabled`, or all of
Smoothness/AntiShimmer/Silhouette/ActorsOnly/WrapEnabled off, checked once per
frame at the §4.1 site (mirror `releaseActorMasks()`); also from
`release_pack` (line 2454) and the shadow/screen release paths. VRAM at 4K:
66 + 2×16.6 + 2×2 ≈ 103 MB while enabled.

---

## 7. Settings → uniforms (`settings.xml`, next to `CineRigRimShadow` line 2954 — all `CineRigRim*` live there)

| setting | type | default | range | → |
|---|---|---|---|---|
| `CineRigRimSmoothness` | F32 | 0 | 0..1 | `rig_rim_quality.x` |
| `CineRigRimAntiShimmer` | F32 | 0 | 0..1 | `rig_rim_quality.y` |
| `CineRigRimSilhouette` | F32 | 0 | 0..1 | `rig_rim_quality.z` |
| `CineRigRimSilhouetteWidth` | F32 | 3.0 | 0.2..30 | `rig_rim_sil.x` (cm → m, or px → half-res texels, §4.2) |
| `CineRigRimSilhouetteUnits` | U32 | 0 | 0 cm at subject, 1 px@1080p | selects `rig_rim_sil.w` (texels/m or 0) |
| `CineRigRimSilhouetteSoft` | F32 | 0.5 | 0..1 | `rig_rim_sil.y` |
| `CineRigRimSilhouetteGap` | F32 | 0.08 | 0.01..0.5 (no UI) | `rig_rim_sil.z` |
| `CineRigRimSilhouetteSeedThreshold` | F32 | 0.02 | 0.002..0.2 (no UI) | `rig_rim_aux_params.y` |
| `CineRigRimSilhouetteSeedSoft` | F32 | 0.5 | 0.02..1 (no UI) | `rig_rim_aux_params.z` |
| `CineRigRimActorsOnly` | BOOL | false | | `rig_rim_quality.w`, `rig_rim_wrap2.w`, subject source (§4.1) |
| `CineRigRimActorsOnlyAlpha` | BOOL | false | Phase 2, greyed out | (unused) |
| `CineRigRimWrapEnabled` | BOOL | false | | runs C1–C3 |
| `CineRigRimWrapStrength` | F32 | 0.25 | 0..1, step 0.01 | `rig_rim_wrap.x`, squared in shader |
| `CineRigRimWrapWidth` | F32 | 2.0 | 0.5..8 (× band width) | `rig_rim_wrap.y` reach texels |
| `CineRigRimWrapBlur` | F32 | 1.0 | 0..3 | `blur_q = value` (quarter-res texels per tap, both axes) → `rig_rim_wrap2.xy` of C2 / C3a |
| `CineRigRimWrapSaturation` | F32 | 0.7 | 0..1 | `rig_rim_wrap.z` |
| `CineRigRimWrapClamp` | F32 | 1.0 | 0.5..1 (no UI) | `rig_rim_wrap.w` |
| `CineRigRimDebugView` | U32 | 0 | 0 Off, 1 Rim only, 2 Silhouette band, 3 Smooth normal, 4 Subject mask, 5 Wrap only | §4.4 |

Director: append the new keys at `llfloaterdirector.cpp` 1030–1057 and the
reset branch at 1360+; reset list `alpanelcinelightrig.cpp` 671–700.
`RIG_RIM_PRESETS` are **not** extended (user decision). Status refresher at 2 Hz,
on-screen only, diff-guarded (CLAUDE.md draw() rules); the card is
`cine_rig_rim_quality_card` (340×352) after `cine_rig_rim_card`,
`enabled_control="CineRigRimEnabled"`, `fit_cards` flow, `xui_layout_check.py`
zero findings.

---

## 8. Debug-view encodings

| view | where | encoding |
|---|---|---|
| 1 Rim only | light shaders (`rig_rim_mode == 2`) | existing |
| 2 Silhouette band | rigRimDebugF | seed pixels red; near-side band green = `band·near_ok`; far side blue = `band·(1−near_ok)`; geometry with no edge in the 24-texel window dark grey 0.05; sky black |
| 3 Smooth normal | rigRimDebugF | `n_geo * 0.5 + 0.5` (eye space) |
| 4 Subject mask | rigRimDebugF | grey 0/1 |
| 5 Wrap only | rigRimWrapF, blend off, `rig_rim_wrap2.z = 1` | the additive wrap term on black |

---

## 9. OFF-path invariant

- `rig_rim_quality == vec4(0)` ⇒ in `rigRimTermEx` every `if (rig_rim_quality.N > 0.0)` is untaken, `n_r == n`, `k_eff` untouched; the remaining statements are `rigRimTerm`'s in the same order ⇒ bit-exact. `rigRimTerm()` itself is unchanged and stays as the reference. The forward-alpha wrappers (`RimSplit/RimKeep/RimKeepRef` without `px`) forward `rigRimNeutralPixel(n)`; the ALR parity permutation (`ALR_RIMSPLIT_PARITY`) compares the split path against the reference **with the same `px`**, so T21 parity holds with quality on.
- `rig_rim_mode == 0` ⇒ no pixel is loaded and no texture tap runs — the pre-existing guard. (The forward shaders now evaluate `length(fwidth(norm))` unconditionally at main scope, review P2-5; the value is unused unless `rig_rim_mode != 0`, so the colour math is unchanged.)
- **Pre-existing bug fixed in passing (review P2-10):** the `pbrCalcPointLightOrSpotLightRimKeepRef` pair in `deferredUtil.glsl` was behind `#ifdef ALR_RIMSPLIT_PARITY`, but that is a per-program permutation (`llviewershadermgr.cpp` 2295/2382) which never reaches the once-compiled utility object — so the T21 parity build of pbralphaF / sharedActorFxPbrF could never have linked (and would have failed the whole deferred load, since `gDeferredPBRAlphaProgram` is not soft-fail). The guard is removed; the functions are now always defined and dead-stripped from every non-parity program. Behaviour with `AvatarSilhouetteRimParity` off is unchanged.
- No pre-pass runs and no target is allocated unless `CineRigRimEnabled` and a quality/wrap setting is non-zero.
- `applyOnLens`, bloom, exposure, G-buffer, velocity, ReShade capture: untouched by items 1–3; the wrap adds HDR light before metering exactly like weather (documented in §4.3).
- **Proof:** back up the exe as `AlchemyTest.pre-rimq.exe`; with `CineRigRimEnabled` on, all quality settings 0 and wrap off, byte-compare a deterministic snapshot against the backup (same scene, fixed exposure, animation paused), and once more with `CineRigRimEnabled` off. Compiler re-ordering across the new branches is the residual risk (design risk 7); the snapshot diff is the only accepted evidence.

---

## 10. Perf targets and Tracy zones

Targets (4K, 5090): A ≈ 0.07 ms, B1–B3 ≈ 0.08 ms (B2/B3 are 2×49 texelFetches at half-res with early-out), per light program +0.02 ms (two taps), C1–C3 ≈ 0.07–0.1 ms. Zones (GPU, `LL_PROFILE_GPU_ZONE`): `"rig rim aux"`, `"rig rim sil seed"`, `"rig rim sil dt"`, `"rig rim wrap blur"`, `"rig rim wrap"`, `"rig rim debug"`. CPU: `LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - rig rim aux")` and `("renderFinalize - rig rim wrap")`.

---

## 11. Shader-side algorithm notes (for the reviewer; the C++ needs none of this)

- Smooth normal: 5 depth taps (centre ±x ±y), min-|Δz| derivative per axis, `cross`, flipped to face the camera; if it still disagrees with the shading normal (`dot < 0`) the shading normal is used instead (two-sided / normal-map extremes). Degenerate (zero) cross ⇒ shading normal. Sky ⇒ neutral pixel (`|n̄| = 1`, subject 0). *Deviation:* the design said "flipped toward the camera and gb.normal"; flipping away from the camera is never right for a visible surface, so the disagreement case falls back instead.
- Toksvig |n̄|: average of the shading normals of the centre and the depth-continuous neighbours only (`|Δz| < 0.04·z + 0.02 m`), so a silhouette does not read as "noisy" and soften the rim exactly where it lives. *Deviation (additive):* the design said "average of 5 decoded normals".
- Anti-shimmer: `ft = len / (len + k_eff·(1−len))`, `k_eff = max(k_eff · mix(1, ft, AntiShimmer), 0.05)`.
- Silhouette band: `band = 1 − smoothstep(W(1−soft), W(1+soft), dist)`, `near_ok = 1 − smoothstep(gap, 2gap, |z − z_seed| / z_seed)` — *symmetric* tolerance (design wrote `z vs seed_z·(1+gap)`; a pixel on a nearer object next to someone else's silhouette is rejected too, which the one-sided test would admit). `W = min(width_texels(z), 24/(1+soft))`.
- Seeds: rotoRing residual test at radius 1 half-res texel (2 px), 8 taps, near side = seed; sky never seeds. The tangent plane uses the **smooth normal from the aux map** (not the normal-mapped shading normal). Grazing taps (`|n·ray| ≤ 0.02`) and taps whose plane hit is behind the camera are **skipped**, never predicted with `z_c` (review B3: that fallback made every pixel of distant ground/water a seed beyond ~85 m). Why flat ground never seeds now: for a plane the predicted depth along the neighbour's ray *is* the neighbour's depth, so `resid = 0` at any distance up to depth-buffer quantisation; a metric floor (`resid > 0.01 m`) absorbs that quantisation before the relative threshold is applied. Sky seed depth is `6e4` (finite in fp16, review P2-7).
- DT: exact windowed EDT; H pass stores offset `(±i, 0)` and distance `|dx|`, V pass offset `(dx_h, ±i)` and `sqrt(dH² + dy²)`, R = 24 half-res texels, early-out when `i ≥ best`.
- **One-nearest-seed limitation (review P2-10/9):** the DT keeps a single nearest seed per pixel, so where another near-side silhouette is closer than the pixel's own (an arm crossing the torso, a prop in front of the face) that seed wins; its depth fails `near_ok` for the torso pixel, so the torso's rim is *suppressed* in a band of width W around the arm rather than widened. Accepted in Phase 1; a two-seed DT would double the sil-map cost.
- Wrap: `dir` = the DT offset direction (aspect-corrected) when the seed is ≥ 1.5 texels away, else `n_geo.xy` (the only stable outward direction at the seed itself, review P2-1); sample the quarter-res blur at `uv + offset·texel + dir·reach`, clamp per channel, desaturate, `× strength × band × near_ok × (1 − |n_geo·v|) × subject^gate`, additive. Reads depth through `deferredUtil` at the renderFinalize slot exactly as `renderCineOutline` does (same `inv_proj` hazard class — see the Night Mask comment in `postEffectUtilsF.glsl`; accepted precedent). All aux/sil/blur reads are `textureLod(…, 0)` (non-uniform flow, review P2-6).
- Light shaders: the aux pixel is loaded after the cone / attenuation `discard`s in spotLightF (review P2-8); pointLightF / multiPointLightF have no earlier discard.

## 12. Risks for review

1. Texture units on the light programs (§3 fallback — must test `mActiveTextureChannels`, a link does not fail on overflow).
2. GL state in C1–C3/D inside `renderFinalize` (blend enable, colour mask, draw-buffer narrowing) vs the beauty pass and the sidecar attachments.
3. 1-px features in the depth-normal reconstruction (blend-only mitigation via `Smoothness < 1`).
4. Fast-bind staleness of `rig_rim_quality` / `rig_rim_sil` (§4.2).
5. Positional reserved-uniform list — count-verify 14 entries in both files.
6. Half-res sil map: `prox` has a 2-px staircase at the band edge (soft hides it); tile seams on tiled snapshots (same as Roto Ink).
7. Bit-exactness under compiler reordering — snapshot diff is the proof.
8. `fwidth()` in the forward shaders is now evaluated unconditionally at main scope (hoisted, review P2-5) — residual cost a few ALU ops in mode 0.
9. Forward-alpha `rig_rim_quality` upload lives in `ALCineRigRim::bindGlobals` (§4.2, review B2) — if WP2 forgets it, Smooth/Steady silently do nothing on alpha; the in-world check is a normal-mapped alpha-blend surface with Smooth = 1.
10. One-nearest-seed suppression next to overlapping limbs (§11).

## 13. Wrap-strength preset retune (2026-10-23)

| Preset | Amount old → new | Clamp old → new |
|---|---:|---:|
| Subtle Edge | 0.25 → 0.12 | 2 → 1 |
| Soft Portrait Wrap | 0.65 → 0.33 | 4 → 1 |
| Strong Backlight | 0.55 → 0.28 | 6 → 1 |
| Hair Shine | 0.25 → 0.12 | 4 → 1 |
| Silhouette Glow (any angle) | 0.8 → 0.4 | 6 → 1 |
| Neon / Stage | 0.25 → 0.12 | 8 → 1 |
| Dreamy Halo | 1.1 → 0.55 | 6 → 1 |
| Noir Kicker | 0.25 → 0.12 | 2 → 1 |
| Two-Sided Kick | 0.25 → 0.12 | 4 → 1 |
| Fashion Edge | 0.35 → 0.17 | 4 → 1 |
| Sci-Fi Hologram | 0.7 → 0.35 | 6 → 1 |
| Candle Glow | 0.5 → 0.25 | 4 → 1 |
| Metal Sheen | 0.25 → 0.12 | 4 → 1 |
| Skin Sheen | 0.25 → 0.12 | 3 → 1 |
| Soft Fill Wrap | 0.65 → 0.33 | 4 → 1 |
| Horror Edge | 0.25 → 0.12 | 4 → 1 |
| Moonlit Rim | 0.35 → 0.17 | 3 → 1 |
| Glass Shell | 0.25 → 0.12 | 4 → 1 |
| Tron Suit Kick | 0.25 → 0.12 | 6 → 1 |
| Ares Red Backlight | 0.25 → 0.12 | 6 → 1 |
| Golden Hour Flare | 1.4 → 0.7 | 8 → 1 |
| Glacial Frost Edge | 0.25 → 0.12 | 2 → 1 |
| Strobe Cut | 0.25 → 0.12 | 4 → 1 |
| Aquatic Caustic Rim | 0.9 → 0.45 | 6 → 1 |
| Ember Lick | 0.75 → 0.38 | 5 → 1 |
| Overcast Diffusion | 0.6 → 0.3 | 2.5 → 1 |
| Clamshell Beauty Edge | 0.5 → 0.25 | 3.5 → 1 |
| Limelight Beam | 0.8 → 0.4 | 7 → 1 |
| Crypt Pale Glow | 0.7 → 0.35 | 3 → 1 |
| Rain Slick Glaze | 0.25 → 0.12 | 4 → 1 |
| Dusk Horizon Kick | 1.1 → 0.55 | 6.5 → 1 |
| Spectral Silhouette | 1.6 → 0.8 | 4.5 → 1 |
| Natural Separation | 0.3 → 0.15 | 3 → 1 |
| Dialogue Two-Shot | 0.3 → 0.15 | 3 → 1 |
| Interview Hair Light | 0.35 → 0.17 | 3.5 → 1 |
| Window Backlight | 0.5 → 0.25 | 5 → 1 |
| Overcast Outdoor | 0.2 → 0.1 | 2 → 1 |
| Night Street Edge | 0.3 → 0.15 | 5 → 1 |
| Lamp Practical | 0.4 → 0.2 | 4 → 1 |
| Dark Background Lift | 0.25 → 0.12 | 3 → 1 |

## 14. Pre-alpha wrap history, debug and rasterization (2026-10-23)

This scheduling/resource description supersedes the older renderFinalize-only
wrap descriptions in §§4.3–5. C1–C3 run at the pre-alpha pool boundary in
`renderGeomPostDeferred`, with the live projection, before haze/water/hair.
Beauty adds the term there so subsequent alpha coverage occludes it.

SSR still captures at its existing post-alpha boundary. When this main view
produced wrap, its color source is the existing `rigRimWrapScene` pre-alpha
snapshot; its depth source remains the post-alpha screen. Thus history excludes
wrap and alpha color on wrap-enabled frames. The existing color/depth blit is
split into two calls, without another image copy, target or increase in copied
pixel bytes. Wrap/Rim off uses the original screen color/depth copy. Validity
resets at every MAIN lighting entry, including early-outs and disabled frames.

Debug view 5 shades C3 once at that same pre-alpha boundary into the optional
full-res RGBA16F `rigRimWrapDebug`, then copies its RGB late using `gCopyProgram`.
It never blurs hair/weather or recomputes the term late. Cost: one display draw
and 8 bytes/pixel (63.3 MiB at 3840×2160), allocated on first use of view 5 and
retained until target cleanup/resize. The indexed state transaction saves both
polygon modes, forces GL_FILL for all wrap/blur/display draws, then restores the
incoming modes, alongside blend/masks/routing. Auxiliary MRTs and glow survive.

`rim_alpha_gl_check.py` checks production shaders, the extracted C++ state
transaction, SSR helper and capture call against real WGL FBOs. It covers
successive reflective frames, a camera cut with history retained, Wrap/Rim
toggle-off on the next frame, exact post-alpha depth, cached debug identity,
wireframe, both blur formats, and the existing halo/strength/off-byte gates.
# Smooth normal revision (2026-10-24)

This revision supersedes the original depth-normal smoothing description below.
`CineRigRimSmoothness` still defaults to 0. The stock auxiliary permutation and
pass-demand predicates are unchanged at zero; no smoothing targets or draws are
created. `CineRigRimSmoothRadius` defaults to 12 cm (1–50 cm); the surface-space
kernel projects through the current projection/depth and is bounded to 32 pixels
per axis. This bound can reduce the requested world radius in close shots.
`CineRigRimTextureNormals` defaults to 0 (0–1), preserving existing presets'
removal of normal-map detail as Smoothness increases.

When smoothing is requested, the auxiliary guide permutation selects coplanar
depth quadrants rather than mixed triangle derivatives. Two full-resolution
separable surface filters blur shading and geometric normals. Depth continuity,
geometric-normal agreement and subject identity stop filtering across boundaries;
every intervening pixel is inspected. A compact continuous kernel, fixed pixel
grid and current-frame inputs avoid random/temporal sampling phases.

The horizontal scratch is RGBA32F: two exactly packed 12-bit-per-axis octahedral
normals plus shading/geometry edge covariance moments. The vertical pass uses
those moments to distinguish flat shading from interpolated vertex normals and
transfers shading perturbations by shortest-arc rotation onto the smooth base.
Constant triangle steps contribute no texture detail. At Texture normals=1,
normal-map detail is retained. Forward alpha retains its interpolated vertex
base and scales its existing smoothing contribution by (1−Texture normals).
The full-resolution RGBA16 published auxiliary texture retains the original
layout. Toksvig is computed from the final Smoothness/detail normal, excluding
depth-boundary derivatives. Debug 3 displays that same final Smoothness blend.
Allocation/compile failures retain shading normals rather than geometric facets.

Both new settings belong to rim snapshots, reset and scene state; older scenes
reset omitted keys to their declared defaults. The two Advanced kit rows live
beside Smoothness and are indexed for smooth normals, texture normals, facets
and blocky. Reproduce the GPU regression with
`python scripts/code_tools/rim_normals_gl_check.py --timing`; use
`--shader-ref b0d3b1ac2d0` for the failing baseline. Artifacts are written under
`.tmp/rim_normals/`.
