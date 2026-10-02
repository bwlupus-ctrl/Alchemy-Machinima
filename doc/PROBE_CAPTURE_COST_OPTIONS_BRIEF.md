# Probe capture cost options: design brief (R1)

Two opt-in settings that cut the cost of capturing reflection-probe cube faces:
`RenderProbeSunShadows` and `RenderProbeCaptureScale`. This is a design only.
Code is read at HEAD `4fbd8b7a6b9`, and every claim below cites `file:line` there.
Labels: **PROVEN** means the cited code shows it. **INFERENCE** means it is reasoned
from code but not observed in-world.

Paths are abbreviated as follows: `pipe` = indra/newview/pipeline.cpp, `pipe.h` = pipeline.h,
`disp` = llviewerdisplay.cpp, `rmm` = llreflectionmapmanager.cpp, `hero` = llheroprobemanager.cpp,
`vwin` = llviewerwindow.cpp, `app` = llappviewer.cpp, `shu` = app_settings/shaders/class1/deferred/shadowUtil.glsl.

## §0 Codex R1 resolution (verdict: GO-after-fixes)
| ID | Finding | Resolution |
|---|---|---|
| A1 | Reuse must sample each main map with its real size and its legacy or world-unit kernel | §1.4: containment selection kept; each cascade sampled per kernel at its own size; subject column through `sunBandC` |
| A2 | Shared shadow state and uploaded uniforms leak out of opt-in faces | §1.3: save/restore everything around every opt-in face (Full fallback included), then force a slow re-bind |
| A3 | `age <= 1` freshness is unsafe | §1.4: per-iteration token, invalidated at `app:1558`, published at the end of the main generation; no previous-frame reuse |
| A4 | Off called `skipRenderingShadows` (needs an FBO) and still projected | §1.5: one uniform returns lit before any projection; whole option gated on deferred and shadow detail > 0 |
| B | Known limits not collected; VRAM implied as a saving | §6 Known limits (VRAM quantified as a net increase) |
| C | Uniform registration, permutations, capture-pack pointer, default validation | §1.6, §2.1, §8 item 1 |

## 0. Verified facts
- **F1. There are only two `generateSunShadow` call sites (PROVEN).** The main view calls it at `disp:841`,
  inside `if (!for_snapshot)` (`disp:836`) with `gFrameCount > 1` (`disp:838`). Every cube face calls it
  at `disp:1336`, inside the Tracy zone "cube face shadow" (`disp:1335`). Prism/VCam aux views never call
  it. They sample the main maps instead (`pipe:26678`, `pipe:22625`). Prism has its own spot-only
  generator (`pipe:28143-28169`).
- **F2. Frame order (PROVEN).** `gFrameCount++` happens at `disp:602`. Hero/mirror probes render at
  `disp:745-751`, before the main sun shadows (`disp:841`); the Prism aux view renders after them
  (`disp:917`). `display()` runs at `app:1560`, and then "df Snapshot" runs the probe update at
  `app:1564-1566`. So ordinary probe faces render **after** the main view of the **same** iteration.
- **F3. A cube face does NOT overwrite the main view's shadow MAPS (PROVEN).** `updateProbeFace` swaps
  `mRT = &mAuxillaryRT` (`rmm:2830`) and swaps it back at `rmm:2869`. Hero probes swap to `mHeroProbeRT`
  (`hero:301`, `hero:305`). Maps are per pack (`pipe.h:1230`), and both the writes (`pipe:27820-27833`) and
  `getSunShadowTarget` (`pipe:26678`) go through `mRT`. The meta that is written is the aux pack's
  (`pipe:27063`, `pipe:27807`).
- **F4. A cube face DOES overwrite shared, non-pack shadow state (PROVEN).** That state is
  `mSunShadowMatrix` (a global array sized `MAX_SHADOW_MATS = 4 + MAX_SPOT_SHADOWS`, `pipe.h:1562`,
  `pipe.h:1373`), with entries [0..1] written at `pipe:27801` and projector entries [4..] re-expressed
  with the probe face's inverse view at `pipe:28072-28073`. It also includes
  `mShadowModelview/Projection` (`pipe:27799-27800`, `pipe:28078-28079`) and `mSunClipPlanes`
  (`pipe:26996-27047`, stretched at `pipe:27050-27055`). Probes fit only 2 cascades (`pipe:27197`,
  `pipe:27076`). The main view rebuilds all of this at `disp:841`.
  INFERENCE: `display(for_snapshot=true)` (`vwin:5771`, `vwin:6004`) skips generation (`disp:836`), so it
  can consume these leftovers. This is pre-existing and has not been verified in-world.
- **F5. How cube-face lighting binds shadows (PROVEN).** A cube face uses `gDeferredSunProbeProgram`
  (`pipe:23913`; `sunLightF.glsl`, `llviewershadermgr.cpp:2811-2817`). The slow bind binds the maps
  (`pipe:22953`, `pipe:22499-22512`), uploads `shadow_matrix` from `mSunShadowMatrix`
  (`pipe:22958-22967`) and `shadow_clip` (`pipe:23001`), then calls `uploadShadowUniforms`
  (`pipe:23005`). The fast bind (`pipe:22799-22813`) re-binds the maps and calls `uploadShadowUniforms`
  but does **not** re-upload `shadow_matrix`/`shadow_clip`. `uploadShadowUniforms` uploads
  units/coef/size scalars per (pack, serial) stamp (`pipe:22628-22669`; coef and size only when units are on,
  `pipe:22642-22662`). It returns early unless engaged (`pipe:22671-22678`). Forward and alpha passes run
  inside the face too (`pipe:24731`).
- **F6. Shader (PROVEN).** Legacy selection compares eye-space z against `shadow_clip` (`shu:556-627`).
  Legacy `pcfShadow` divides by `w` unchecked (`shu:183-186`), has no uv guard, and uses `shadow_res`
  for every cascade (`shu:239-245`). That value is cascade 0's size (`pipe:23082-23085`), yet each cascade
  can have its own size (`pipe:1810-1822`). The units/subject kernel is `sunBandC`/`pcfShadowC`, with w and
  uv guards and per-cascade coef and size (`shu:362-436`, `shu:266-353`). Maps use `TAM_CLAMP` with
  `LEQUAL` (`pipe:1893-1896`).
- **F7. Prism precedent (PROVEN).** Re-expressing the main cascades for an aux camera is off by default,
  because the legacy path samples clamp-to-edge (`llprismlens.cpp:7484-7511`, `settings.xml:17908-17911`).
- **F8. Probe shadow maps are sized from the capture pack (PROVEN).** `pipe:1793-1795`; the aux pack is
  `mProbeResolution * 4` (`pipe:1246-1248`), giving about 528² at the default.

## 1. Option 1: `RenderProbeSunShadows` (U32: 0 = Full, 1 = Reuse main view, 2 = Off)

### 1.1 Scope
Add `U32 LLPipeline::mProbeSunShadowMode`. `updateProbeFace` sets it from the setting, using an RAII scope
around both `probe->update` calls (`rmm:2849-2866`). It is forced to 0 unless
`sRenderDeferred && RenderShadowDetail > 0`, the same gate as `pipe:26753`. Every reflection-map-manager
capture funnels through that function: ordinary (`rmm:2740`), realtime (`rmm:941`), the Cinematic Live
Probe (`rmm:1017`, `rmm:936`), and on-demand slicing (`rmm:2303`, `rmm:2329`).
**Hero/mirror probes are excluded and always run Full.** `hero:295-305` never sets the flag. Mirrors are
viewed directly and render before this iteration's main maps exist (F2).

### 1.2 The face (`disp:1334-1337`)
```
mode = gPipeline.mProbeSunShadowMode;                  // 0 for setting 0, hero, shadow detail 0
if (mode == 0) { generateSunShadow(cam); }             // DEFAULT: stock statement, nothing else
else {
  ScopedProbeShadowState saved;                         // 1.3, live until end of display_cube_face
  if (mode == 1 && !gPipeline.mainSunShadowsPublishedThisIteration()) mode = FULL_FALLBACK;  // 1.4
  if (mode == FULL_FALLBACK) generateSunShadow(cam);
  else { reexpressSpotShadowMatricesForCurrentView(); gPipeline.mProbeShadowSampleMode = mode; }
}
```
The scope must hold through `renderDeferredLighting` (`disp:1388`, including forward passes at
`pipe:24731`) and release at the end of `display_cube_face` (`disp:1396-1397`).
`reexpressSpotShadowMatricesForCurrentView()` mirrors the Prism Stage-1 loop
(`llprismlens.cpp:7512-7540`): `bias * mShadowProjection[k] * mShadowModelview[k] * inverse(view)` for
live `mShadowSpotLight` slots, when `RenderShadowDetail > 1` (`pipe:27843`). Without it, projector
shadows in probes would keep the main view's matrices (`pipe:28072`).

### 1.3 State save/restore for opt-in faces (A2)
- `ScopedProbeShadowState` saves `mSunShadowMatrix[0..MAX_SHADOW_MATS-1]` (projector entries 4..13 included),
  `mShadowModelview[..]` and `mShadowProjection[..]` (`pipe.h:1562-1564`), `mSunClipPlanes` (`pipe.h:1571`),
  and `mProbeShadowSampleMode`. It restores all of them at scope end, for Reuse, Off and the Full fallback.
- A CPU restore does not restore uniforms already uploaded into programs bound during the face, and
  fast binds never re-upload `shadow_matrix`/`shadow_clip` (F5). So after the restore, set
  `mCanBindFast = false` on every `LLGLSLShader::sInstances` entry, with the same loop as
  `pipe:22562-22567`. The next bind of each program is then a slow bind, which re-uploads from the
  restored arrays (`pipe:22958-23005`).
- Setting 0 takes none of this path. The stock face, including its leftovers (F4), is unchanged.

### 1.4 Reuse (A1, A3)
- **Freshness token (A3).** Clear `mMainSunShadowToken = false` at the start of the render iteration
  (`app:1558`, before `display()` at `app:1560`). Set it true at the **end** of `generateSunShadow`
  (`pipe:28132-28140`) only when `mRT == &mMainRT && !gCubeSnapshot`. Reuse requires the token **and**
  `mMainRT.shadowMeta.validCascades > 0` (`pipe.h:1248`; a reset meta on reallocation, `pipe:1800`).
  Anything else falls back to Full, including:
  - the first frames (`disp:838`);
  - a skipped `display()`;
  - the empty-point-cloud early return (`pipe:26936-26956`), which never reaches the end of the function.
  The sun-black path clears the maps and does reach the end (`pipe:27091-27094`), so Reuse correctly
  samples them as lit. There is no previous-frame reuse, and therefore no region-shift invalidation is
  needed.
- **Binding.** While the sample mode is 1, `getSunShadowTarget` (`pipe:26678`) and the pack in
  `uploadShadowUniforms` (`pipe:22625`) resolve to `mMainRT`, exactly as for `sPrismLensRender`. The stamp is
  keyed by (pack, serial) (`pipe:22628-22630`), so it then carries the **main** meta's units, scope and
  coef, which describe the maps being sampled.
- **Matrices (kept).** `M_j = mat4(sunVP_main[j] * inverse(dmat4(current modelview)))`, composed at bind
  time with the probe-face camera current. `sunVP` is the view-independent double matrix from
  `pipe:27808` (`sunVP[0] = columnVP` under Follow subject, `pipe:27416-27417`). This is the same
  composition as `pipe:22683-22693`.
- **Uniforms.** These are new and dedicated, so they never write the stock `shadow_matrix` that fast
  binds skip:
  - `shadow_probe_mode` (int, default 0);
  - `shadow_reuse_matrix[4]` (mat4);
  - `shadow_reuse_valid` (vec4: `j < validCascades && fitted[j]`, the rule at `pipe:22692`);
  - `shadow_reuse_res` / `shadow_reuse_res_h` (vec4: the real `mMainRT.shadow[j]` width/height).
  `uploadShadowUniforms` uploads `shadow_probe_mode` on **every** slow and fast bind, **before** its
  early return at `pipe:22671`. The uniform value cache makes the repeated 0 free. The matrices, valid
  flags and sizes are uploaded only when the mode is 1. If the meta is units, `shadow_cascade_coef` is
  already uploaded by the stamp (`pipe:22642-22649`).
- **Shader branch.** Inside `#if defined(SUN_SHADOW)`, the first statement of `sampleDirectionalShadow`
  checks the mode (Off = 2 is in 1.5): `if (shadow_probe_mode == 1) return sampleDirectionalShadowReuse(...)`.
  For j = 0..3, unrolled, with `valid[j]` checked, it takes the **first cascade that contains the point**:
  - **Subject column** (j = 0 and `shadow_subject.x > 0.5`): `sunBandC(shadowMap0, M_0,
    shadow_cascade_coef[0], res0, true, ...)`. This is the main view's own column call (`shu:748-749`) and keeps
    its receiver-plane bias (`shu:360-432`).
  - **World-unit cascade** (`sunUnitsCascade(j)`, or j = 0 with units on, as `shu:439-442`, `shu:712-714`):
    `sunBandC(map_j, M_j, shadow_cascade_coef[j], res_j, true, ...)`.
  - **Legacy cascade:** `pcfShadowRes(map_j, norm, M_j * p_leg, res_j, ...)`. This is a copy of `pcfShadow`
    (`shu:180-250`) taking `res_j` instead of `shadow_res`, called only after the containment test has
    established `w > 1e-4`.
  - **Containment** is tested on the same coordinate each kernel uses: the legacy-offset position for legacy
    cascades (`shu:548-552`, `shu:648`) and the un-offset position for units (`shu:650`). It requires
    `w > 1e-4`, `uv` in `[pad, 1 - pad]` with `pad` = kernel footprint / `res_j` (`shu:735-740` gives the
    footprint per kernel), and `z` in `[0, 1]`.
  - If no cascade contains the point, the result is 1.0 (lit). There is no cross-cascade blend and no
    far-fade term (seams: §6).

### 1.5 Off (A4)
- Off never touches maps or framebuffers. In particular it does not call `skipRenderingShadows`
  (`pipe:29345-29354`), which binds FBOs. At shadow detail 0 the sun targets are released
  (`pipe:1830-1836`), and 1.1's gate keeps Off from ever being reached there.
- `uploadShadowUniforms` uploads `shadow_probe_mode = 2` on both bind paths. The very first statement of
  `sampleDirectionalShadow` is `if (shadow_probe_mode == 2) return 1.0;`, before any projection,
  `/w` or texture read, which removes the unchecked divide at `shu:184`. Every sun-shadow consumer in the face
  (the sun program and the forward and water passes, F5) is therefore lit. Spot shadows are unchanged
  (`sampleSpotShadow`, `shu:760`). Volumetric samplers do not run in cube faces (`pipe:18375`,
  `pipe:19574`).

### 1.6 Default 0, and registration (C)
- With setting 0 the face runs the stock statement only. `shadow_probe_mode` is uploaded as 0, which
  equals its initialiser.
- The shader text gains an untaken branch and the uniforms. So the default is **output-identical by
  construction but not binary-identical**, and it **must be validated in-world** (§8 item 1).
- Register the new names as file-scope `LLStaticHashedString`, beside `pipe:499-508`, as ShadowDist did.
  If an implementer prefers reserved uniforms instead, append the enum (`llshadermgr.h:181` pattern) and
  the `push_back` (`llshadermgr.cpp:1474` pattern) at matching positions, at the end of the list.
- No new permutation is needed. If one is added, it goes **after** that program's `clearPermutations()`
  (`llviewershadermgr.cpp:1426`, `:1454`, `:5247` precedents).

## 2. Option 2: `RenderProbeCaptureScale` (U32, clamped to {1, 2, 4}, 4 = stock)

### 2.1 Every place that assumes 4×
| Site | What | Change |
|---|---|---|
| `pipe:1246-1248` | aux pack at `mProbeResolution * 4` | keep this stock (2.2) |
| `rmm:359-364` | `mRenderTarget` Gaussian scratch at `res * 4` | `res * scale` |
| `rmm:2869`, `rmm:2901` | `mRT` restored to main, then `screen_rt = &mAuxillaryRT.screen` | save `capture_pack = gPipeline.mRT` before `rmm:2869`; use `&capture_pack->screen` (C) |
| `rmm:2906` | Gaussian `resScale = 1/(2*res)` (`gaussianF.glsl:48`) | keep it; same UV footprint |
| `rmm:2938`, `rmm:2946` | mip 0 is one bilinear tap; `resScale` unused (`reflectionmipF.glsl:28-36`) | none |
| `vwin:6055`, `vwin:6124` | capture viewport = pack `deferredScreen` width | none; follows the pack |
| `rmm:2994-3060`, `pipe:5225`, `rmm:267` | radiance/irradiance, `glViewport`, tiny-cull and dirty sizes use the output res | none |
| `pipe:1794` | probe sun maps sized from the pack (F8) | 2.3 |
| hero | own pack (`pipe:1250-1256`), own 1× `mRenderTarget` (`hero:109`), own downsample (`hero:328-383`) | none |
Precedent: hero faces already run this same downsample code from a 1× capture (`pipe:1253`, `hero:341`).

### 2.2 Rejected: shrinking `mAuxillaryRT`
It hosts dynamic-texture previews, which assert width ≥ 512 (`lldynamictexture.cpp:194-202`, `pipe:461`),
and GLTF previews (`llgltfmaterialpreviewmgr.cpp:458`, `:514`). At the default probe res of 128
(`settings.xml:20749-20758`), shrinking would clip previews, and clamping back to 512 saves nothing.

### 2.3 Dedicated capture pack
- `RenderTargetPack mProbeCaptureRT` (next to `pipe.h:1282-1285`) is allocated in the aux block of
  `allocateScreenBufferInternal` (`pipe:1236-1260`) only when `scale != 4`, at `mProbeResolution * scale`. It
  uses the same `mRT` swap and `gCubeSnapshot` hack, and is excluded from post/bloom like `mHeroProbeRT`
  (`pipe:1353`).
- Its sun maps are allocated at the stock 4× size (`allocateShadowBuffer` at `pipe:1406`). Option 2 changes only
  colour/G-buffer resolution; Option 1 owns shadow cost.
- `rmm:2830` selects `mProbeCaptureRT` when `scale != 4` and the pack is complete, and `mAuxillaryRT` otherwise.
- Add the pack to every release site: `pipe:2302-2304`, `pipe:2330-2332`, `pipe:23222-23224`.
- The aux size is also scaled by `RenderResolutionDivisor`/`Multiplier` (`pipe:1266-1279`), and the new pack
  inherits that.

### 2.4 Reallocation and clamp
`RenderReflectionProbeResolution` has **no listener** ("requires restart", `settings.xml:20752`; read only at
`rmm:3526`). Register `RenderProbeCaptureScale` with `handleReflectionProbeDetailChanged`
(`llviewercontrol.cpp:668-681`, registered like `:1189-1197`), which rebuilds the GL buffers via `pipe:2398`.
Extend the release condition at `rmm:3533-3537` to a changed scale. Clamp:
`v >= 4 ? 4 : (v >= 2 ? 2 : 1)`. At 4 nothing new is allocated and every 2.1 row resolves to its stock
value, so the default is byte-identical C++ with no shader change.

## 3. UI (Lightbox Rendering; after `floater_lightbox_settings.xml:1538-1593`, before `:1594`)
Two `combo_box` rows with `allow_text_entry="false"` and a `LightBox.ResetControlDefault` reset button
(precedent `:3560-3562`, `alfloaterlightbox.cpp:67`).
- **Probe sun shadows:** Full (stock) / Reuse main view / Off.
  Tooltip: "How reflection probes get sun shadows. Full (stock): each probe face renders its own, which is the
  most accurate and the most expensive. Reuse main view: probes borrow your camera's shadows, which is much
  cheaper. Areas your camera's shadows don't cover (often behind you) are lit, and probes can show shadows
  of things they don't draw, such as avatars. Off: probes get no sun shadows, which is cheapest.
  Interiors in reflections and ambient light get brighter. Mirrors are not affected."
- **Probe capture resolution:** 4x (stock) / 2x / 1x.
  Tooltip: "How sharply probes are rendered before they are shrunk. 4x (stock) is the smoothest. 2x
  costs about a quarter of the pixel work and 1x about a sixteenth, with more shimmer. It saves little
  when probe capture is CPU-bound. It uses a few MB MORE video memory, not less. Changing it rebuilds
  the probes and mirrors (brief hitch)."

## 4. Settings and presets
Add `RenderProbeSunShadows` (U32 0) and `RenderProbeCaptureScale` (U32 4), both Persist 1, next to
`settings.xml:20805-20815`. Add both to `getGraphicsControlNames` (`llpresetsmanager.cpp:342-351`). Read the
mode with `LLCachedControl` in `updateProbeFace`; it needs no listener.

## 5. Interactions
- On-demand, slicing and every-N (`rmm:432-444`, `rmm:2303`) all go through `updateProbeFace`, so the
  setting applies identically.
- The Live Probe in Every frame, On change and Manual (`rmm:603-612`, `rmm:1132-1136`) captures through
  `rmm:941`/`rmm:1017`. Reuse suits it best, because the actor is normally inside the main cascades.
- A scale change resets the manager (`rmm:3540`, `rmm:3580-3581`), so a Manual Live Probe must be
  re-captured.

## 6. Known limits (B, accepted, not blockers)
1. **VRAM is a net increase (INFERENCE, approximate).** The aux pack stays allocated (2.2). The capture pack
   at 256² (probe 128, scale 2) holds about 36 B/px: G-buffer per `pipe:619-631` + depth + `screen` + `deferredLight`,
   about 2.4 MB, plus 4 stock-size depth maps of about 528², about 4.5 MB. Shrinking `mRenderTarget` saves about
   1.5 MB. The net is about **+5 MB**, a bit less at 1×. Never present this as a saving.
2. **Cascade-boundary seams.** Reuse picks the first containing cascade with no blend.
3. **Coverage depends on the camera.** Main cascades are fitted to the main frustum (`pipe:26931-26996`, far
   ≤ 512 m at `pipe:26977`), so geometry outside them, often behind the camera, is lit under Reuse.
4. **Shadows without casters.** Non-dynamic probes hide avatars (`vwin:6099-6108`), and the default probe
   draws sky, water, terrain and clouds only (`rmm:2846-2847`). Full shadows come only from what the probe
   draws (`pipe:26777-26845`); the main maps hold every caster.
5. **Capture-time coverage persists.** It lasts until the next refresh under on-demand, On change or Manual.
   INFERENCE: On change hashes scene and light state (`rmm:634-640`), not the camera.
6. **Off brightens interiors**, because sun light is no longer occluded.
7. **The scale rebuild disturbs mirrors.** `handleReflectionProbeDetailChanged` also resets the hero
   manager and reloads shaders (`llviewercontrol.cpp:675`, `:678`), so mirrors and all programs briefly
   rebuild.

## 7. Tracy expectation
- Reuse/Off: the `generateSunShadow` zone count per frame drops from about 1.6 to 1.0 (plus hero faces).
  "cube face shadow" (`disp:1335`) drops to near zero. Each opt-in face then adds one slow bind per
  program on its next bind (1.3), so watch "cube face light" (`disp:1387`) and the main deferred pass
  for that.
- Scale 2/1: the GPU time of "cube face geom"/"light" (`disp:1380-1388`) heads toward ¼ and 1/16 when
  fill-bound; cull and sort (`disp:1327`, `disp:1342`) don't change. Compare the CPU and GPU spans of
  "cube face geom" first to tell whether the 15 ms face is CPU-bound.

## 8. In-world test list
1. **Defaults (0/4) A/B against the prior build:** the same probes, the same zone counts. This is required,
   because default equivalence is only argued, not proven.
2. Reuse, facing a lit courtyard, then turning 180°: shadows should be close to Full in front, and absent
   behind the camera (expected).
3. Reuse with an avatar in the open and dynamic probes off: check for a floating avatar shadow.
4. Off indoors at midday: brighter, no NaN, black or sparkle pixels. Also check at `RenderShadowDetail` 0.
5. Projector shadows in probes under Reuse and Off match Full.
6. Mirrors are unchanged in all modes.
7. Live Probe in Every frame, On change and Manual, crossed with the three modes; then on-demand with slicing.
8. After a probe update, the main view and a high-res snapshot are unaffected (1.3 restore).
9. Follow subject and world units with Reuse: probe shadows sane and the main view untouched.
10. Per-cascade `BDMergeShadowResolution0..3` set to different sizes, with Reuse: no misaligned kernels.
11. Scale 4→2→1→4 live: no crash, probes re-converge, appearance-editor and GLTF previews not clipped.

## 9. Least-sure list (reviewers check these first)
1. Does every program that samples sun shadows in a cube face reach `uploadShadowUniforms`
   (`pipe:23005`, `pipe:22813`)? If one does not, it would see a stale `shadow_probe_mode`.
2. Cost of the `mCanBindFast` sweep (1.3) on every opt-in face. Would a per-face list of bound
   programs be enough?
3. The uniform budget in alpha and water programs (4 mat4 + 3 vec4 + 1 int on top of `shadow_matrix[14]`, `shu:53`).
4. The kernel-footprint `pad` for the soft and Vogel kernels (`shu:735-740`) when used for containment.
5. Whether the token's publish point (`pipe:28132-28140`) is reached on every successful main generation path.
6. `allocateShadowBuffer` at a size different from its pack (2.3), versus the `mRT->width/height` users
   (`pipe:1263-1264`, `pipe:1132`).
