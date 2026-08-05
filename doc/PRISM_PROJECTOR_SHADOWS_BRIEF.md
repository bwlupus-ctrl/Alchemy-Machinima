# Prism projector/spot SHADOWS in the aux capture — implementer brief

Make projector/spotlight **shadows** render inside the Prism auxiliary capture (camera feed + surface
lens), keeping the MAIN view **byte-identical** (Prism-off and main-view-during-aux both unchanged).
Projector *lights* already render in the feed; only shadows are missing/wrong. Baseline: current
working tree (Option B pool rework + Option C optics + Gemini's `calcNearbyLights` spot-admit, all
already reviewed). Read **`GEMINI.md`** + **`doc/AI_AGENT_CODEBASE_ORIENTATION.md`** first. **Do NOT
build / commit / `git add`.** Deliver compile-clean, `/WX`-clean source. Warnings are errors.

## Why shadows are wrong today (verified)
- The Prism aux pass (`llprismlens.cpp` `renderAuxiliaryView` ~4479-4720) renders geom+lighting but
  **generates no shadow maps**, and spot-shadow slot assignment is frozen (`!sPrismLensRender` gates
  at `pipeline.cpp` ~16351, ~16447, ~16469, ~17196).
- `setupSpotLight`'s slot search (~17167-17175) runs unconditionally, so a projector that holds a
  **main-view** slot `i` gets `PROJECTOR_SHADOW_INDEX=i` and the shader samples the main-view
  `mSpotShadow[i]` with `mSunShadowMatrix[i+4]`. But `mSunShadowMatrix[i+4] = trans*proj_light*
  view_light*inv_view` (built at ~19407) embeds the **main** camera's `inv_view`, while the shader's
  `pos` is **aux** view-space → the shadow is **geometrically wrong** in the feed. A projector with
  no main slot renders **fully unshadowed**.

## The key facts that make this safe & cheap
1. A spot-shadow map is rendered from the **projector's own frustum** → its content is
   **camera-independent**. The projector's light view/proj are persisted per slot in
   `mShadowModelview[i+4]` / `mShadowProjection[i+4]`.
2. The only camera-dependent term in the sampling matrix is `inv_view`. So for a projector that
   already has a resident main-view map, we only need to **recompute the matrix** with the aux
   camera's `inv_view` — no new render pass.
3. **Frame order (verified, llviewerdisplay.cpp):** `generateSunShadow` @841 → `renderAuxiliaryView`
   @917 → main `stateSort` @931 → main deferred lighting (later). The aux pass runs AFTER main
   shadows are built but BEFORE the main view consumes them, and `ScopedPrismRenderState`'s dtor
   (which calls `endPrismAuxiliaryState`) fires at the end of `renderAuxiliaryView` (@917) — i.e.
   **before** @931. Therefore any in-place mutation of shared shadow *scalars/matrices* during aux,
   **restored in `endPrismAuxiliaryState`**, is invisible to the main view. The only thing that
   canNOT be cheaply restored is the `mSpotShadow[]` **texture content** (main consumes it later this
   frame) — so aux generation must use **dedicated** targets, never `mSpotShadow[]`.

## Containment scaffold that already exists
`beginPrismAuxiliaryState` (`pipeline.cpp` ~9308) / `endPrismAuxiliaryState` (~9441) already
save+restore `mNearbyLights`, `mShadowSpotLight[]`, `mTargetShadowSpotLight[]`, `mSpotLightFade[]`,
`mPoissonOffset`, light masks, GL light snapshot, sun/moon dir+diffuse, and the reflection-probe UBO.
`ScopedPrismRenderState` saves camera, all matrices, `mRT`, viewport/scissor/clear/colormask/blend,
`sVisibleLightCount`, bound target, `sCurResX/Y`, current shader. **You will ADD** save/restore for
the shadow matrices you mutate (below).

---

## STAGE 1 — reuse main maps, fix the transform (smallest correct shadow; MUST be independently correct)
Gives a correct feed shadow for every projector that already holds a valid main-view slot. Zero new
render passes. Cannot regress the main view.

1. **pipeline.h:** add member `glm::mat4 mPrismSavedSunShadowMatrix[MAX_SPOT_SHADOWS];` next to the
   existing `mPrismSaved*` spot members (~1254-1272). (Match the real element type of
   `mSunShadowMatrix` — mirror its declaration.)
2. **`beginPrismAuxiliaryState`** (near the existing spot save at ~9334): for `i` in
   `[0, MAX_SPOT_SHADOWS)` save `mPrismSavedSunShadowMatrix[i] = mSunShadowMatrix[i+4];`.
   **`endPrismAuxiliaryState`** (near ~9453): restore `mSunShadowMatrix[i+4] =
   mPrismSavedSunShadowMatrix[i];`.
3. **`renderAuxiliaryView`** — insert a block **immediately before `updateCull`** (~4700), after the
   aux camera/matrices/water/clip are installed. Guard the whole block on
   `LLPipeline::sRenderDeferred && RenderShadowDetail > 1` (match the main-view spot-shadow gate at
   ~19227). Then:
   - `const glm::mat4 inv_view_aux = glm::inverse(get_current_modelview());`
   - build the **same** `trans` bias matrix `generateSunShadow` uses at ~19407 (read that line and
     replicate it exactly — do not invent a different bias).
   - for `i` in `[0, gPipeline.bdmergeMaxSpotShadows())`: if `mShadowSpotLight[i].notNull() &&
     mSpotShadow[i].getWidth() > 0`, set
     `mSunShadowMatrix[i+4] = trans * mShadowProjection[i+4] * mShadowModelview[i+4] * inv_view_aux;`
   (Everything else already works: `setupSpotLight` finds the slot, `bindShadowMaps` binds
   `mSpotShadow[i]`, `bindDeferredShader` uploads the now-aux-correct matrix.)

**Stage-1 invariant:** the only shared state touched is `mSunShadowMatrix[4..9]`, restored in `end`
before @931 → main byte-identical. No alloc, no pass, no NaN (guarded on `getWidth()>0`; matrices are
finite because the aux projection/modelview were validated earlier in `renderAuxiliaryView`).

---

## STAGE 2 — generate shadows for feed projectors lacking a main slot (full coverage)
Layer on top of Stage 1. Adds dedicated aux shadow targets + a contained generation pass so ANY feed
projector casts a shadow. Structure so Stage 1 remains functional if Stage 2 is reverted.

1. **pipeline.h members:** `LLRenderTarget mPrismSpotShadow[MAX_SPOT_SHADOWS];` and save arrays
   `glm::mat4 mPrismSavedShadowModelview[MAX_SPOT_SHADOWS];`,
   `glm::mat4 mPrismSavedShadowProjection[MAX_SPOT_SHADOWS];` (match the real element types of
   `mShadowModelview`/`mShadowProjection`).
2. **Alloc/release:** allocate `mPrismSpotShadow[i]` lazily at the **same resolution** as `mSpotShadow`
   (mirror the `mSpotShadow` allocation ~1585-1611 and its release in `releaseSpotShadowTargets` +
   the GL-teardown path). Guard use with `getWidth()>0` (mirror ~19336-19341).
3. **Redirect binding under aux** (parallel to the existing sun branch):
   - `getSpotShadowTarget(i)` (~18451): `return sPrismLensRender ? &mPrismSpotShadow[i] :
     &mSpotShadow[i];`
   - `DEFERRED_PROJ_SHADOW_RES` upload (~16027): under `sPrismLensRender` read `mPrismSpotShadow[0]`
     size (harmless if equal, correct if it diverges).
4. **Save/restore additions:** in `begin/endPrismAuxiliaryState` also save+restore
   `mShadowModelview[i+4]` and `mShadowProjection[i+4]` for `i` in `[0, MAX_SPOT_SHADOWS)` (Stage 1
   already handles `mSunShadowMatrix[4..9]`; `mShadowSpotLight/mTargetShadowSpotLight/mSpotLightFade`
   are already saved).
5. **Generation block** — same insertion point as Stage 1 (before `updateCull` @4700), and it
   **replaces/subsumes** Stage 1's per-slot matrix loop for the slots it fills. Same
   `sRenderDeferred && RenderShadowDetail > 1` gate. Steps:
   a. **Stateless projector selection** — iterate `mLights` directly, apply the SAME spot/validity
      filter the Prism branch of `calcNearbyLights` uses (~9081-9133: is-light, not HUD, attachment
      rules, radius/color, spotlight), choose up to `bdmergeMaxSpotShadows()`. **Do NOT call
      `updateSpotLightPriority`** (it mutates persistent `LLVOVolume::mSpotLightPriority`, which is
      NOT saved). Selecting from `mLights` (not `mNearbyLights`) keeps it independent of aux
      light-list timing.
   b. Push+AND the shadow render-type mask exactly as `generateSunShadow` does (~18544; pop at end
      like ~19468); `setColorMask(false,false)`.
   c. For each chosen slot `i`: `mShadowSpotLight[i]=drawable; mSpotLightFade[i]=1.f;` replicate the
      projector-frustum math (~19361-19396) to get `view`/`proj`; store `mShadowModelview[i+4]=view`,
      `mShadowProjection[i+4]=proj`, `mSunShadowMatrix[i+4]=trans*proj*view*inv_view_aux`; guard
      degenerate projectors (finite check; `tanf(fov*0.5f)` not ~0; nonzero scale) and skip
      `mPrismSpotShadow[i].getWidth()==0`; `mPrismSpotShadow[i].bindTarget()`+viewport+`clear()`;
      `gPipeline.mShadowRenderSpotLight/RenderSpotLight = drawable` (mirror ~19433 — the cull uses it
      to skip the projector's own geometry); `renderShadow(view, proj, shadow_cam, aux_result[i],
      /*use_shader depth_clamp*/false, /*do_cull*/true)`; reset the RenderSpotLight sentinel to null;
      `flush()`.
   d. For **unused** slots `i` in `[chosen, MAX_SPOT_SHADOWS)` set `mShadowSpotLight[i]=NULL` so
      `setupSpotLight` returns `s_idx=-1` (unshadowed) for absent projectors.
   e. `aux_result` must be `static LLCullResult aux_result[MAX_SPOT_SHADOWS];` (size MAX — the old
      `[2]` overflow crash was fixed at ~19429; do not reintroduce it).
   f. Pop the render-type mask; **re-install** the aux state before `updateCull`: aux
      modelview/projection (`gGL.loadMatrix` + `set_current_*`), `gGLViewport` + `glViewport(0,0,
      render_width,render_height)`, and `LLViewerCamera::sCurCameraID = CAMERA_PRISM_LENS`.

**Stage-2 hazards & neutralization:**
- Main-view content corruption → generate into `mPrismSpotShadow[]` (NOT `mSpotShadow[]`); all mutated
  shared scalar/matrix arrays are restored in `end` before @931. → byte-identical main view.
- `renderShadow` runs its own `stateSort` (~18040) → generating BEFORE the aux `stateSort` (@4701)
  means the aux draw maps are rebuilt afterward; do not move generation after the aux stateSort.
- Persistent priority leak → stateless selection, no `updateSpotLightPriority`.
- Crash/NaN/OOB → degenerate-projector guards; clamp to `bdmergeMaxSpotShadows()` (≤ MAX_SPOT_SHADOWS);
  `getWidth()>0` skip; `aux_result[MAX_SPOT_SHADOWS]`.
- Projector volumetrics consume spot state only when `!sPrismLensRender` (restored) → no conflict.
- Honor `RenderShadowDetail > 1`: if the user's spot shadows are globally off, the feed shows none too.

## Invariants (do not weaken)
`setViewNoBroadcast` only; full `ScopedPrismRenderState` restore on every exit; one aux scene render
per frame (the shadow gen is extra depth-only passes, still one aux *scene* render); composite samples
linear-HDR before tonemap; **Prism-OFF byte-identical**; main-view-during-aux byte-identical; default
behavior unchanged when `RenderShadowDetail<=1`; don't touch clone/overlay/`actorghostF`/`LLActorMover`;
no shader edits are required (spotLightF.glsl/shadowUtil.glsl already sample `shadowMap4-9` via
`shadow_matrix[4+idx]`); `/WX` clean (cast `LLSD::Real`→`F32`, cover switch cases, no unused vars).

## Deliver
Edited files + a per-file note of what changed at which line, delineating Stage 1 vs Stage 2 so Stage
2 can be reverted independently. Confirm: main view byte-identical (all mutated shadow scalars/matrices
restored before llviewerdisplay.cpp:931; generation writes only `mPrismSpotShadow[]`); no
`updateSpotLightPriority` during aux; `aux_result` sized MAX_SPOT_SHADOWS; degenerate guards present;
`RenderShadowDetail>1` gate honored; `git diff --check` clean. Claude re-reviews (focus: byte-identical
main view, no leak, no crash) and builds.
