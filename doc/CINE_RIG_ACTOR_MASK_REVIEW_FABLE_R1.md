# Cine Rig Actor Mask: adversarial engine-design review (Fable R1)

**Date:** 2026-10-01
**Reviewed:** `doc/CINE_RIG_ACTOR_MASK_BRIEF_V3.md` against HEAD `057e55885f9` (read-only; no edits, build or commit).
**Scope:** WP1 mask pass/target, WP2 actor-only deferred lights, WP3 forward gating, WP4 probe/view exclusion.
WP0 code in the working tree was not reviewed.

**Labels:** PROVES = read in code at HEAD. INFERENCE = reasoning not checked in code or in-world.

## Verdict: GO-after-fixes

The approach is right: under the hard constraints (G-buffer layout, depth format, no existing shader or
program changes) a redraw of the recipients against the final G-buffer depth is the only route, and the
fullscreen-only light routing is sound. Four A items would each break the core promise as written:

- **A1** the mask is cleared with the engine's magenta clear colour, so R8 = 255 and every pixel passes.
- **A2** the mask programs never receive `mirror_flag`/`clipPlane`, so `mirrorClip` is inert in mirror and Prism views.
- **A3** the system-avatar alpha stream (hair, skirt, eyelashes) is a forward actor stream the brief never selects a set for.
- **A4** the mask avatar vertex shader is modelled on the shadow family, which lacks `AVATAR_CLOTH`; depth agreement
  needs a per-program "same expression as the G-buffer shader" rule, not a family template.

All four have one-paragraph fixes. Everything else is B (document) or C (nit).

---

## 1. Mask correctness against the G-buffer (WP1)

### A1. Clear colour (PROVES). Blocker.

`LLRenderTarget::clear()` is a bare `glClear(mask & mask_in)` (llrendertarget.cpp:569-596): it uses the current
`glClearColor` and honours the current colour mask. The G-buffer clear sets `glClearColor(1,0,1,1)` (llviewerdisplay.cpp:1030
main view, :1373 cube faces; grey `0.5` under wireframe) and nothing resets it before `renderDeferredLighting()`'s own first
reset at pipeline.cpp:23096-23098, which is *after* the point where §5.2/§5.4 place the mask pass. §5.3 says only
"`actorMask.clear(GL_COLOR_BUFFER_BIT)`". With R cleared to 1.0 the byte is 255, `(b & bits) != 0` is true everywhere, and every
actor-only light lights the whole set. The Debug=1 overlay would show it, but the brief's own text produces it.

Also: at the end of the G-buffer the colour mask is `(true,false)` (llviewerdisplay.cpp:1034); `renderDeferredLighting` resets it
at :23066, again after the mask pass site.

**Fix (WP1):** in the mask pass, before the clear: `gGL.setColorMask(true, true); glClearColor(0,0,0,0);` then
`actorMask.clear(GL_COLOR_BUFFER_BIT)`; restore the previous clear colour on exit (the engine convention is the
`glClearColor(1,1,1,1) ... glClearColor(0,0,0,0)` pair at :23096-23098). Add "clear colour" to the §7 reviewer checklist and
make the instrument's ISOLATION-DEFERRED precondition include "mask byte histogram has a zero bin" (a 255-everywhere mask
makes ISOLATION fail loudly, which is good, but a histogram line names the cause).

### A2. `mirror_flag` and `clipPlane` never reach the mask programs (PROVES). Blocker for mirror/Prism.

`mirrorClip()` (globalF.glsl:35-44) discards on `mirror_flag > 0`. Those uniforms are uploaded only by
`LLSettingsVOSky::applySpecial` (llsettingsvo.cpp:1312, :1315), which runs from `LLGLSLShader::bind()` only when
`mUniformsDirty` (llglslshader.cpp:1088-1092). `mUniformsDirty` is set exclusively by `LLEnvironment::update` over
`LLViewerShaderMgr::mShaderList` and its rigged/GLTF variants (llenvironment.cpp:1676-1690), and by the Prism's
`collectEnvironmentShaders` (llprismlens.cpp:6448-6452). A program not pushed onto `mShaderList` starts with
`mUniformsDirty = false` (llglslshader.cpp:348) and keeps the GLSL default `mirror_flag = 0` forever. The G-buffer programs are in
the list (llviewershadermgr.cpp:734-738, :1893); the shadow family that §5.4 uses as the template is not, because shadows do not clip.

Consequence in a mirror (`MASK` view): a recipient standing behind the mirror plane is removed from the G-buffer by
`mirrorClip`, but the mask redraws it unclipped. Its depth is *nearer* than the visible set behind it, so the LEQUAL test passes
and the mask marks set pixels. The actor-only light then lights the set inside the mirror, exactly the leak the brief promises
cannot happen. The same applies to the Prism lens clip (llsettingsvo.cpp:1294-1306).

**Fix (WP1):** push all six mask programs onto `mShaderList` where the actorghost programs are pushed
(llviewershadermgr.cpp:617-626); rigged variants are dirtied with their parent (llenvironment.cpp:1681-1683). Verify at
runtime in the WP1 report: `getUniformLocation(MIRROR_FLAG) >= 0` for every mask program (globalF.glsl is a precompiled
library object listed at llviewershadermgr.cpp:1324, so the symbol links as long as the mask fragment shaders declare
`void mirrorClip(vec3)` and call it). Add a verdict: a `MASK` mirror view with `mirror_flag == 0` on any mask program is
`ACTORMASK NO-CLIP` and the view falls to `EXCLUDE`.

### A4. Depth agreement is a per-program expression rule, and the avatar template is wrong under cloth (PROVES the shader text; INFERENCE on invariance).

The G-buffer vertex transforms at HEAD:

| Geometry | G-buffer program / expression |
|---|---|
| Static | diffuseV:68, pbropaqueV:88, materialV:93, bumpV:69: `modelview_projection_matrix * vec4(position,1)` |
| Rigged (`HAS_SKIN`) | all four: `mat = modelview_matrix * getObjectSkinnedTransform(); pos = mat * p; gl_Position = projection_matrix * pos` (pbropaqueV/materialV/bumpV go through a `vec3 pos` + `vec4(pos,1)`; w is exactly 1 because `ret[3] = vec4(trans,1)` in objectSkinV:67, so this is the same arithmetic) |
| System body | avatarV:133-142: `pos.x = dot(trans[0], pos_in)` (row dots), `gl_Position = projection_matrix * pos`; under `AVATAR_CLOTH` the vertex is first displaced by the wind block (avatarV:73-131) |
| Eyes (rigid) | `gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram` (lldrawpoolavatar.cpp:754) = static form |

The shadow templates the brief names: shadowAlphaMaskV (static and `HAS_SKIN`) and shadowSkinnedV reproduce the static and
rigged forms exactly. avatarShadowV:41-54 reproduces the body form **without** cloth. `gDeferredAvatarProgram` gets
`AVATAR_CLOTH` whenever `RenderAvatarCloth` is set (llviewershadermgr.cpp:3625-3627) and the pool uploads
`AVATAR_WIND/SINWAVE/GRAVITY` (lldrawpoolavatar.cpp:1152-1175). A mask body shader without the cloth path puts every
clothing-weighted vertex at its undisplaced position: the mask then misses the displaced skirt/lower body by the wind
amplitude, far more than any polygon offset. That is holes on the actor, not speckle.

Invariance: GLSL does not guarantee the same `gl_Position` across programs unless both declare `invariant gl_Position`, which
the G-buffer shaders do not (and must not change). In practice the same expression text compiled by the same driver gives the
same code; the residual risk is FMA contraction, worth a few float32 ULPs, which at the far end of a 24-bit buffer is a few depth
ULPs. That is what the polygon offset is for (see C1 for the default).

**Fix (WP1):** state the rule explicitly: *each mask vertex shader uses the identical `gl_Position` expression, uniform set and
helper file (`objectSkinV.glsl`, `avatarSkinV.glsl`) as the G-buffer program that drew the geometry.* Concretely: static and
rigged from shadowAlphaMaskV's two branches; body from avatarV's transform **including the `AVATAR_CLOTH` block**, with the
permutation mirrored from gDeferredAvatarProgram and the three wind uniforms uploaded as the actorghost replay already does
(llactormover.cpp:15122-15146); eyes from the static form. The WP1 report lists each mask program's expression beside its
G-buffer counterpart (rule 6: verify by reading, not by compiling).

### B/C items for §1

- **B1-mask. Shared-depth lifecycle (PROVES, sound).** `release()` on a target that only shares depth detaches it
  (llrendertarget.cpp:473-487); `shareDepthBuffer` LL_ERRS on `mDepth` or `mUseDepth` (:430-438), so release-first is
  mandatory as written. `clear(GL_COLOR_BUFFER_BIT)` strips the depth bit (:573-586; N1 holds). `GL_R8` is accounted
  (:60-61) and `addColorAttachment(GL_R8)` with `GL_RGBA/GL_UNSIGNED_BYTE/NULL` is a legal TexImage call. The aux and
  hero packs go through `allocateScreenBufferInternal` (`mRT` swap at pipeline.cpp:1241/1249), so the top-of-function release
  covers them; `release_pack` (:2299-2311) and `release_prism_scratch_entry` (:1574-1586) are the other two sites, as the brief says.
- **B2-mask. Alpha-mask cutoffs (PROVES).** Body and eyes use `sMinimumAlpha = 0.2` (lldrawpoolavatar.cpp:66, :757, :910);
  the deferred body pass runs `renderSkinned()` with `sSkipTransparent = true` (:286), so system hair/skirt/eyelashes are not
  in the G-buffer and must not be in the mask: the `renderSystemActorGhost` pattern already sets `sSkipTransparent = true`
  (llactormover.cpp:15158). Attachment draws use `params.mAlphaMaskCutoff` (lldrawpoolalpha.cpp:1119 shows the PBR form).
- **B3-mask. LOD, palette, timing (PROVES).** Rigged palettes are cached per `gFrameCount` (llvoavatar.cpp:11416-11434), so
  the mask pass and the G-buffer pass of the same view upload identical palettes; the body palette is rebuilt from joint
  world matrices times `LLDrawPoolAvatar::getModelView()` at every `uploadJointMatrices` (llviewerjointmesh.cpp:99-115),
  which also honours the client outer transform (`LLScopedAvatarOuterModelView`, llvoavatar.cpp:6183-6212) because
  `renderSkinned()` installs it. A second `renderSkinned()` in the same frame does not rebuild (llvoavatar.cpp:6239-6250:
  `mDirtyMesh` is already 0). Nothing between `renderGeomDeferred` and `renderDeferredLighting` changes LOD.
- **B4-mask. `RenderDepthPrePass` (PROVES, document).** When on (llviewerdisplay.cpp:1044-1066), simple/fullbright/shiny
  depth is written by `gOcclusionProgram` first. It is the static MVP form, so the rule in A4 still holds; note it in the WP1 report.
- **C1. Polygon offset default.** With factor 0, `-k` units pulls the mask by `k/2^24` of window depth. In linear metres that
  is about `k * z^2 / (near * 2^24)`: for k = 4 and a 0.25 m near plane, 24 um at 5 m and 2.4 mm at 50 m. No leak risk at a hand
  on a shoulder even at k = 16 (1 cm at 50 m). The only failure direction is holes when the redraw lands more than k ULPs
  behind, so the safe default is larger: start the bisect at **-8** (GUESS, same as the brief's) and keep the range -64..0.
- **C2. `texelFetch(ivec2(gl_FragCoord.xy))`.** Valid because every target the lighting pass draws into has a 0-origin
  full-size viewport (`bindPrismLensTarget` is exact-size, pipeline.cpp:1739-1744; tiled snapshots change the projection,
  not the viewport). If a sub-rect viewport ever appears, subtract `viewport.xy`. One comment line.
- **C3. Mirror at `RenderReflectionProbeLevel == 0`.** The local-light block is skipped in every cube face
  (pipeline.cpp:23342-23343), so the mirror shows the actor unlit by the actor-only light while the main view shows it lit.
  Stock behaviour for all local lights; add one sentence to B5.

## 2. Fullscreen-only routing for actor-only lights (WP2)

### Does PARITY prove point-light equivalence? Yes for the shader math (PROVES), with one instrument hole (C4).

Compared pointLightF.glsl (volume) with multiPointLightF.glsl (fullscreen), both at HEAD:

| Term | Volume | Fullscreen | Equal? |
|---|---|---|---|
| Light vector | `trans_center - pos` (vertex-transformed centre, pointLightV:43) | `light[i].xyz - pos` (CPU `mul_mat4_vec3(mat, tc)`, pipeline.cpp:23510-23513) | same matrix; float rounding only |
| Range test | `lightDist >= size` discard (:96) | `dist <= 1.0` (:119, :151) | boundary differs only where attenuation is already 0 |
| Attenuation | `calcLegacyDistanceAttenuation(dist, falloff)` | same, `falloff = light_col.a` (:112, :159) | identical |
| PBR | `pbrPunctual(..., normalize(lv), ...)`, `* 3.25`, `specPunc *= 1 - gb.envIntensity` (:117-126) | same (:125-131) | identical, including the ALR Tame carrier |
| Legacy diffuse | `nl` from `calcHalfVectors`, eps-clamped to 1e-6 (deferredUtil.glsl:386-388); no front-face gate | `if (dot(n, lv) > 0)` gate (:153-154) then the same `calcHalfVectors` | differs by at most `1e-6 * color * diffuse` on back-facing pixels |
| Legacy specular | identical formula (:160-174 vs :166-179) | | identical |
| Rig Rim | `rigRimTerm(n, v, l, dist_atten*color, ...)` (:153) | `rim_l = lv/length`, same attenuation (:190-192) | identical |
| Zero write | legacy branch discards all-black (:187-190) | writes 0 | `BT_ADD` of 0 is identical |
| Extra | none | `pos.z < far_z` discard (:80-83), `far_z = min(light.z - light.w)` from real lights | pixel beyond every sphere: contribution already 0 |

The 1e-6 term is three orders below the PARITY tolerance `1e-3 + 0.01*max`, so PARITY is a valid gate. The ALR carrier is
read in the PBR branch of both and in neither legacy branch, so there is no asymmetry there either.

- **C4. PARITY can pass vacuously.** F3 routes a point through the volume path only when the camera is outside its box
  (pipeline.cpp:23464-23466); inside, F3 and F1 both use the fullscreen path and PARITY compares a program with itself. The
  instrument must record which path each F3 point light took and return INCONCLUSIVE ("no volume-path point in F3") when
  none did. Spots need no such note: both spot programs compile spotLightF.glsl and differ only in where `center` comes from
  (spotLightF:53-57, :119-123), confirmed.

### Wrappers (PROVES, sound)

- `createShader` passes `mDefines` to every listed file (llglslshader.cpp:466-469), and `loadShaderFile` emits
  `#define main actorMaskedMain` into the extra-code block (llshadermgr.cpp:728-735). multiPointLightV.glsl and pointLightV.glsl
  have **no** `[EXTRA_CODE_HERE]` marker; the loader then prepends the extra block after the version header
  (llshadermgr.cpp:897-915), so the vertex `main` is still renamed and the vertex wrapper is required exactly as Codex R1 #2 said.
- Both wrappers see the define at their marker and `#undef` it; cross-unit function definition within a stage is what the
  viewer does for every `deferredUtil` helper already, so linking is not novel.
- Library objects are precompiled once without program defines (llglslshader.cpp:569-608) and contain no `main`.
- `hash()` covers `mDefines`, `mShaderFiles` and source text (llglslshader.cpp:2067-2097), so the binary cache keys are distinct.
- **C5.** Copy the stock light programs' feature flags verbatim (`isDeferred, hasFullGBuffer, hasShadows, hasSrgb`,
  llviewershadermgr.cpp:2639-2721) and call `add_common_permutations`; otherwise `rigRimMode()`, the shadow samplers and the
  TronA0 `GET_GBUFFER_FLAG` preamble differ from the bodies being wrapped. Also bind through `bindDeferredShader`
  (pipeline.cpp:22665+) like the stock programs; these programs are deliberately *not* in `mShaderList`
  (no match for `gDeferredLightProgram` there), and that is correct because they take no environment uniforms.
- **C6.** spotLightF already carries the G-buffer, `lightFunc`, `projectionMap`, `environmentMap`, `lightMap`, the four sun
  cascades and up to ten spot shadow maps; `createShader` warns above 24 channels (llglslshader.cpp:519-531). The mask
  sampler adds one. Log `mActiveTextureChannels` for both new programs in the WP2 report against
  `gGLManager.mNumTextureImageUnits`.

### LIGHT_COUNT padding (PROVES, sound)

Both loops are `for (i < LIGHT_COUNT)` (multiPointLightF:109, :146). The pad `light = (0,0,1e9,1)`, colour 0, rim 0 gives
`dist = ~1e9 / 1 > 1` and is skipped before any division by `lightDist`. `far_z` from real lights only is right. Cost: 16
iterations per pixel per batch for a handful of bounce omnis, acceptable; note it.

## 3. Forward gating (WP3)

### World vs actor sets, `restoreLightUnitContent`, hash invalidation (PROVES, sound)

- `LLLightState::setPosition`/`setSpotDirection` bake the current modelview in (llrender.cpp:680-689, :736-747) and
  `syncLightState` uploads `mPosition`/`mSpotDirection` raw (:929-945). A snapshot copied raw is therefore valid for every
  draw under the same view matrix. The alpha pool's `applyModelMatrix` changes the model matrix, not the view, and the forward
  shaders consume view-space `light_position[]` (alphaF.glsl:66-69, :373-381), so per-draw model matrices do not invalidate the sets.
- Between capture and draw the view changes only across views: HUD (`sRenderingHUDs` forces byte 0, world set), mirror and Prism
  (each view runs its own `setupHWLights()` at pipeline.cpp:7291, :23076 and :7618 before its alpha draws, so the sets are
  rebuilt under that view's matrix). The 7291 call precedes classification (view is `NONE`), which is harmless because
  `renderGeomDeferred` has no forward-lit draws; the brief should say so to stop a future reader "fixing" it.
- `restoreLightStateSnapshot` already bumps `mLightHash` (llrender.cpp:1661-1668); the new unit-range method must do the same.
  Every draw calls `gGL.syncMatrices()` (llvertexbuffer.cpp:798, :915, :941), which calls `syncLightState()` for shaders with
  `hasLighting || calculatesLighting || calculatesAtmospherics` (llrender.cpp:1119-1122). The deferred alpha, material, PBR alpha
  and avatar alpha programs all set `calculatesAtmospherics = true` (llviewershadermgr.cpp:1941, :2197, :3642), so a hash bump
  between two draws under one bound shader re-uploads before the second draw. The `setRigRimActive` precedent (:888-900) holds.
- `enableLights` only rewrites diffuse when `mLightMask` changes (pipeline.cpp:10755-10777) and reads `mHWLightColors[]`, so
  swapping `mHWLightColors[2..7]` with the set, as §5.7 says, keeps the two in step. `mLightMovingMask` has no consumer
  (set at :10626, saved/restored for Prism only), so the actor sets may ignore it.

### A3. The system-avatar alpha stream is not selected (PROVES). Blocker for classic avatars.

`LLDrawPoolAvatar::renderPostDeferred` (lldrawpoolavatar.cpp:385-399) runs `render(2)` with `sSkipOpaque = true`
(:361) and `gDeferredAvatarAlphaProgram` (:363), which has `calculatesAtmospherics = true` (llviewershadermgr.cpp:3642) and
therefore consumes the GL light units. `renderSkinned()` then draws `renderTransparent()` (llvoavatar.cpp:6400-6404): system
hair, skirt and eyelashes. This runs from `renderGeomPostDeferred`'s pool loop, outside `LLDrawPoolAlpha::renderAlpha`,
so none of §5.7's three selection sites touches it. A recipient with system hair gets the actor-only key on the body and the
world set on the hair.

**Fix (WP3):** in `renderAvatars` on the `is_post_deferred_render` path, after the existing `shouldSuppressSharedActorFx`
test at lldrawpoolavatar.cpp:1132 (so eligibility stays with the real submission, as §5.4.1 requires) and before
`renderSkinned()` at :1179, `selectForwardLightSet(recipientBit(avatarp))` when the view is `MASK`, and restore the world set
after; pass 0 (impostor) stays on the world set (B7). Count the switch in `alpha_switches`.

- **C7.** Actor-only lights skipped in the world loop advance their fade clock only in the actor scan, which runs in `MASK` calls
  (2 of the 3 per main frame; never at :7291). They fade in about 1.5x slower than world lights. Cosmetic; either accept or
  advance the clock in every call. Prism entries are inserted fully faded (pipeline.cpp:10019-10020), so no issue there.

## 4. Bit semantics (PROVES, sound; document)

- **Overlap (hug).** With blending off and `LLGLDepthTest(GL_TRUE, GL_FALSE, GL_LEQUAL)` against the shared G-buffer depth,
  only fragments within the offset of the *visible* surface pass; a recipient behind another recipient's hand fails by far more
  than the offset at any distance (C1 numbers). So the byte is "owner of the visible surface", which is exactly the surface
  the deferred light shades, and any-of against `light_bits` is the right test at depth. Two bits appear in one byte only
  through the worn-animesh union (Codex R1 #6), where both owners legitimately own the surface. Within the offset band (contact
  points, coincident surfaces) last-draw wins: that is B6 and nothing more.
- **Precision.** The fragment writes `byte/255.0`; UNORM8 conversion is `round(v*255)`, exact. `texelFetch(...).r*255.0+0.5`
  then `uint()` recovers `k` exactly for all 0..255. `GL_FRAMEBUFFER_SRGB` (enabled at pipeline.cpp:23266 for another pass) does
  not encode `GL_R8`, which is linear.

## 5. Shadow tiers and probe exclusion (WP2/WP4)

- **Tiers (PROVES, byte-identical for world).** The auction runs inside `setupSpotLight` (pipeline.cpp:24372-24409), which is
  called from the two spot *draw* blocks (:23552, :23654). Because the actor-only block is placed after :23671, every world
  auction completes before any tier-0 candidate appears; tier-0 then takes only remaining null slots and never displaces, so
  `mTargetShadowSpotLight[]` for world and ordinary rig spots is byte-identical to the light-off baseline. Null handling matches
  :24384-24408 as Codex R1 #4 required. **C8:** write that ordering down as an invariant in WP2 (the actor block must stay after
  the world spot blocks), since moving it would silently turn "fills empty slots" into "fills then gets displaced".
- **Prism feed (PROVES).** `generatePrismSpotShadows` admits all spots nearest-first (:27171-27224); sorting actor-only last
  preserves the world choice whenever world spots fill the slots.
- **Probe-centric branch (PROVES).** `isProbeCentricLightCapture()` is true only inside the probe capture scope
  (llreflectionmapmanager.cpp:163, :176); the hero mirror never enters that branch, so `calcNearbyLights` returns early for it
  (pipeline.cpp:9837-9841) and the mirror reuses the main-eye list, which still contains actor-only lights for the `MASK`
  actor scan. The brief's "unless `isMirrorPass()`" clause is therefore dead code, harmless (C9: drop it or keep as a guard).
- **On-demand scheduler (PROVES, answers least-sure 9).** `lightStep` gates every struct/motion/photo event on
  `relevant = s.mEligible || e.mEligible` (alprobeschedule.h:1318-1350): the eligible-to-ineligible transition emits one
  struct event, after which an actor-only light's motion emits nothing. `lightLeave` (:1391-1396) and `liveSelects`
  (:2288-2294) also gate on `mEligible`. Setting `s.mEligible = false` in `probe_gather_light` is sufficient.
- **Live probe (PROVES).** The world-loop `continue` already excludes actor-only lights from every `EXCLUDE` view's deferred
  pass and `setupHWLights`; removing their ids from the pinned list (alcinelightrigmanager.cpp:662-663) is still needed so
  `getCinematicLiveProbePinnedLightCount()` (llreflectionmapmanager.h:148-151) does not inflate `effective_local_light_count`.

## 6. Is the approach wrong anywhere?

No. Given the constraints, a depth-matched redraw into a side target is the only mechanism; a 5th G-buffer attachment
would change every G-buffer shader, stencil would change the depth format, and bounding volumes were rightly rejected.

One alternative is worth naming as a **fallback only**, not a replacement: if test 1's bias bisect shows the shared-depth
LEQUAL-plus-offset route is flaky on the user's driver, the mask pass can instead sample `deferredScreen`'s depth texture in
its fragment shader (it is not attached to the mask FBO, so no feedback loop), compare linear depths with a tolerance in
metres that scales with `z^2`, and drop the shared depth and polygon offset entirely. It costs one depth fetch per mask
fragment and loses hardware early-Z on the mask draws. It is not clearly better today, because the polygon-offset route is
simpler and its only failure mode (holes) is visible in the overlay and tunable; keep it in the brief as "if PARITY/test 1
fail on offset".

## 7. Finding index

| # | Class | Area | One line |
|---|---|---|---|
| A1 | A | WP1 | Mask clear uses the engine clear colour (1,0,1,1): byte 255 everywhere. Set `glClearColor(0,0,0,0)` + colour mask before the colour-only clear. |
| A2 | A | WP1 | Mask programs not in `mShaderList` never get `mirror_flag`/`clipPlane`; mirrorClip inert; mirror/Prism leak. Push them onto the list, verify the uniform, add `NO-CLIP`. |
| A3 | A | WP3 | `LLDrawPoolAvatar::renderPostDeferred` (system hair/skirt/eyelashes, forward-lit) has no set selection. Select `recipientBit(avatarp)` after the :1132 test. |
| A4 | A | WP1 | Avatar mask shader from the shadow template lacks `AVATAR_CLOTH`; rule must be "same expression as the G-buffer program", listed per program in the WP1 report. |
| C1 | C | WP1 | Bias default: start the bisect at -8; only holes are at stake, never leaks. |
| C2 | C | WP2 | `gl_FragCoord` indexing valid today (0-origin viewports); note the `viewport.xy` caveat. |
| C3 | C | doc | Mirrors at probe level 0 skip all local lights; add to B5. |
| C4 | C | WP7 | PARITY must record F3's path and go INCONCLUSIVE if no point used the volume path. |
| C5 | C | WP2 | Copy stock light feature flags + `add_common_permutations`; bind via `bindDeferredShader`. |
| C6 | C | WP2 | Log `mActiveTextureChannels` of the two new programs against the GPU limit. |
| C7 | C | WP3 | Actor-only fade clocks advance in 2 of 3 calls; cosmetic. |
| C8 | C | WP2 | State the ordering invariant: actor spot draws after both world spot blocks. |
| C9 | C | WP4 | `isMirrorPass()` clause in the probe-centric branch is dead; drop or keep as guard. |

Accepted limits B1-B7 were not re-raised. Items marked sound above (shared-depth lifecycle, colour-only clear, UNORM
precision, wrapper mechanics, cache keys, LIGHT_COUNT padding, point-light parity math, raw-snapshot restore, hash
re-upload, tier math, on-demand gating) were each verified against the cited lines and need no change.

## 8. Least-sure for the next reviewer

1. A4's invariance claim is INFERENCE: same expression text gives bit-identical depth on the user's driver. Test 1's bisect
   is the only proof; record the clean bias per geometry class (static, rigged, body, eyes).
2. Whether every forward-lit actor stream is now enumerated: alpha pool natives, interleaved rigged stream, Shared Actor FX
   proxies, and (A3) the avatar pool's post-deferred pass. Grep for other `calculatesAtmospherics` programs that draw avatar
   geometry outside these four.
3. The `mUniformsDirty` path under Prism: `collectEnvironmentShaders` must pick up the new programs from the same list.
