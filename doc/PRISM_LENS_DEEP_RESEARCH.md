> **Status (2026-08-01): AUTHORITATIVE version — supersedes the initial sketch.**
> This is the reviewed deep-research report (GPT), which corrected real errors in the first draft. Claude spot-verified its load-bearing architectural claims against the current tree:
> - `RenderTargetPack` `mMainRT`/`mAuxillaryRT`/`mHeroProbeRT` + `RenderTargetPack* mRT` — `pipeline.h:918-928` ✓ (the real alternate-camera precedent, NOT `mWaterDis`).
> - `renderDeferredLighting()` reads the `LLViewerCamera::getInstance()` singleton (e.g. `pipeline.cpp:12776`, `:14347`) ✓ → a private `LLCamera` alone is insufficient; need a scoped singleton/global-matrix mutation (like `cubeSnapshot()`).
> - `mirrorClip()` + `clipPlane` discard — `class1/deferred/globalF.glsl:32-39` ✓ → reusable behind-lens clip mechanism.
>
> Feature remains QUEUED — after the camera look-at/turn-body build + the Absolute Cinema SL rebrand.

---

# Prism / Magnifying-Glass Lens Prim — Deep Research Report

**Date:** 2026-08-01
**Scope:** Feasibility and implementation path against the current `I:\alchemy-machinima` fork, with illustrative C++/GLSL. This is not a production patch.

## Executive verdict

The feature is feasible in the current OpenGL deferred pipeline, and Tier 1 is the right direction. The cheapest correct first implementation is **not** a stencil pass and is **not** a lone `LLRenderTarget` copied from `mWaterDis`. It is:

1. A dedicated, minimal deferred target pack for one lens.
2. A second scene render from the **same eye point**, using a cropped/off-axis projection centered on the lens's projected footprint.
3. A clip plane at the lens surface, so geometry between the camera and glass cannot reappear inside the magnified image.
4. A composite made by drawing the actual designated face into `mMainRT.screen`, with `GL_LEQUAL` depth testing and depth writes disabled.
5. Main-frame tone mapping, bloom, and post applied once, after the lens has been integrated into the linear HDR scene.

This produces a convincing machinima magnifier/telescope window with correct opaque depth occlusion. Full physical refraction is a different feature: it requires per-ray bending and object-distance-dependent imaging, not an offset camera.

The expected implementation size is medium-to-large: roughly **4–7 engineering days** for a robust single-face MVP and **another 3–6 days** for transparent ordering, multiple lenses, polish, and interaction testing. A full thin-lens/refraction model is a separate research/implementation phase.

## Important corrections to the original brief

### 1. `mWaterDis` is not the whole-scene rerender precedent

`mWaterDis` is allocated as water/atmospheric scratch at `pipeline.cpp:1283-1284`. `doAtmospherics()` and `doWaterHaze()` copy depth into it and sample it at `pipeline.cpp:15829-15849` and `15879-15903`. `mSceneMap` is a main-view SSR copy (`pipeline.h:939-941`), not a general alternate-camera scene target.

The real precedent is the pipeline's `RenderTargetPack` (`pipeline.h:893-928`) and the reflection/hero-probe hot swap:

- `mMainRT`, `mAuxillaryRT`, and `mHeroProbeRT` already hold camera-specific G-buffer/beauty/light targets.
- Reflection probes set `gPipeline.mRT = &gPipeline.mAuxillaryRT` and restore `mMainRT` at `llreflectionmapmanager.cpp:764-798`.
- Hero probes do the same with `mHeroProbeRT` at `llheroprobemanager.cpp:295-305`.
- The simplified alternate-view renderer is `display_cube_face()` at `llviewerdisplay.cpp:1247-1357`.

The prism should follow this architecture, but use a **dedicated minimal pack**. Reusing `mAuxillaryRT` would race with probe updates and ties prism resolution to probe resolution.

### 2. A private `LLCamera` alone is insufficient

Passing a private camera to `renderGeomDeferred(camera)` looks attractive, but the rest of the deferred path still reads the global singleton:

- `renderDeferredLighting()` takes no camera and obtains `LLViewerCamera::getInstance()` at `pipeline.cpp:15151-15180`.
- It later calls `renderGeomPostDeferred(*LLViewerCamera::getInstance())` at `pipeline.cpp:15753`.
- Deferred uniforms read the singleton near plane at `pipeline.cpp:15080-15082`.

Therefore the safe spike is a **scoped mutation of the viewer camera and global matrices**, copied and restored exactly like `LLViewerWindow::cubeSnapshot()` at `llviewerwindow.cpp:6055-6068` and `6186-6191`. Any FOV setter used in that scope must be `setViewNoBroadcast`; `setView` sends `AgentFOV` at `llviewercamera.cpp:801-827`.

The better prism projection described below does not need to modify the camera's FOV field at all: it applies a crop matrix to the saved main projection.

### 3. Main depth testing does not remove foreground geometry from the lens texture

Drawing the lens face with depth testing prevents an opaque foreground object from being overwritten at its true on-screen pixels. It does **not** stop that object from also being rendered by the zoom camera and reappearing magnified or displaced elsewhere inside the lens.

The auxiliary pass must reject everything on the camera side of the lens surface. Use both:

- A user plane in `LLCamera` for coarse CPU/frustum culling.
- A shader clip/discard plane for triangles crossing the surface.

The fork already has a suitable shader path: `mirrorClip()` in `class1/deferred/globalF.glsl` discards fragments on the rejected side, and the mirror plane is supplied in `llsettingsvo.cpp:1113-1149`. Generalize that mechanism into a signed auxiliary-view clip plane instead of introducing `gl_ClipDistance` into every vertex-shader permutation during the MVP.

### 4. `baseFOV / zoom` is not exact zoom math and fails for off-center lenses

Perspective focal length is proportional to `1 / tan(FOV / 2)`; the fork implements that explicitly at `llviewercamera.cpp:183-197`. If a scalar FOV is used, the exact relationship is:

```cpp
const F32 lens_fov = 2.f * atanf(tanf(main_fov * 0.5f) / zoom);
```

More importantly, a narrow symmetric FOV remains centered on the main camera axis. An off-center lens would show the wrong part of the world.

The recommended solution is an **off-axis crop projection**. The code already uses `glm::pickMatrix()` at `llviewercamera.cpp:320-330`. Project the lens face to a pixel rectangle, shrink that rectangle by `1 / zoom`, and prepend a pick matrix to the saved main projection. This keeps the eye and camera orientation unchanged while mapping the desired angular sub-frustum over the entire auxiliary target.

### 5. Moving the eye toward the prim is not "truer optics"

An offset eye is a portal/dolly camera: it reveals different surfaces and creates parallax that a simple magnifier should not. A real converging lens bends a continuum of rays and its image depends on focal length and object distance. A single displaced pinhole view cannot reproduce that.

For the requested "different FOV through a prim" effect, retain the main eye point. Treat physical thin-lens refraction as a later, explicitly different mode.

## Recommended architecture

### Designation model

Store a local descriptor, not a persistent modification to the simulator object:

```cpp
struct PrismLens
{
    LLUUID mObjectId;
    S32    mTE = -1;          // designated texture-entry / face index
    F32    mZoom = 2.f;
    F32    mResolutionScale = 1.f; // relative to projected lens pixel size
};
```

The minimum identity is **object UUID + TE index**. Object-only designation is ambiguous for linksets and makes it difficult to keep the frame/rim material separate from the glass.

At designation time require exactly one selected world object and one selected TE. `LLSelectNode::isTESelected()` and `getLastOperatedTE()` are available in `llselectmgr.h:168-182`; existing face-panel code demonstrates selected-TE iteration throughout `llpanelface.cpp`.

Resolve each frame through `gObjectList.findObject(id)` (`llviewerobjectlist.h:266-278`). Clear or mark inactive when the object is dead, absent, a HUD attachment, lacks the face, is back-facing, or projects below a small pixel threshold. Do not retain raw `LLFace*` across frames.

For the MVP, support one planar volume face and one lens. Curved/nonplanar prim surfaces need a defined optical surface and cannot use one exact clip plane.

### Render targets

Add `mPrismLensRT`, but do not call the normal recursive `allocateScreenBufferInternal()` for it. That allocator also creates post ping/pong, bloom levels, and shadow buffers that the lens pass does not need.

Allocate only:

- `deferredScreen` with the stock deferred attachments.
- `screen` in `GL_RGBA16F`.
- Shared depth from `deferredScreen` to `screen`.
- `deferredLight` only when the selected lighting path needs SSAO/shadow light accumulation.

Illustrative allocation:

```cpp
bool LLPipeline::allocatePrismLensBuffer(U32 width, U32 height)
{
    width  = llclamp(width,  64u, 2048u);
    height = llclamp(height, 64u, 2048u);

    RenderTargetPack& rt = mPrismLensRT;
    if (rt.width == width && rt.height == height && rt.screen.isComplete())
        return true;

    releasePrismLensBuffer();
    rt.width = width;
    rt.height = height;

    if (!rt.deferredScreen.allocate(width, height, GL_SRGB8_ALPHA8, true))
        return false;
    if (!addDeferredAttachments(rt.deferredScreen))
        return false;
    if (!rt.screen.allocate(width, height, GL_RGBA16F))
        return false;

    rt.deferredScreen.shareDepthBuffer(rt.screen);

    if (RenderDeferredSSAO || RenderShadowDetail > 0)
    {
        if (!rt.deferredLight.allocate(width, height, GL_RGBA16F))
            return false;
    }
    return true;
}
```

This mirrors the essential main allocation at `pipeline.cpp:1135-1184` without paying for main-view post resources.

Size the target from the lens's **projected pixel AABB**, not a fraction of the whole screen:

```cpp
target_w = align_up(ll_round(projected_width  * resolution_scale), 8);
target_h = align_up(ll_round(projected_height * resolution_scale), 8);
```

At scale 1.0, the target has roughly one source texel per displayed lens pixel. A screen-wide fraction wastes fill rate whenever the lens is small.

### Projection and auxiliary pass

Let `lens_rect_px` be the bottom-left-origin projected AABB of the actual face. The desired source crop is the same center with dimensions divided by zoom.

```cpp
glm::vec2 center = 0.5f * (lens_min_px + lens_max_px);
glm::vec2 crop_size = (lens_max_px - lens_min_px) / zoom;

glm::ivec4 main_viewport(saved_viewport);
glm::mat4 crop = glm::pickMatrix(center, crop_size, main_viewport);
glm::mat4 prism_projection = crop * saved_main_projection;
```

This is equivalent to a narrower, potentially asymmetric FOV whose optical axis passes through the lens center. It also makes the user-facing zoom value mean what directors expect: `2.0` displays half the angular width and height across the same lens footprint.

The pass should run after object geometry updates and after the main sun/spot shadows have been generated, but **before the main `stateSort()`**. In the current frame loop, the practical window is after the shadow/impostor block (`llviewerdisplay.cpp:833-870`) and before `Display:StateSort` (`llviewerdisplay.cpp:912-925`). The prism's `stateSort()` resets global draw orders; the immediately following main `stateSort()` naturally restores them.

Pseudo-flow:

```cpp
void LLPrismLensManager::renderAuxiliaryView()
{
    if (!shouldRender())
        return;

    ScopedPrismRenderState restore_everything; // camera, matrices, viewport,
                                                // mRT, masks, camera id, flags

    LLPipeline::sPrismLensRender = true;
    LLPipeline::sUseOcclusion = 0; // main-camera queries are invalid here
    gPipeline.mRT = &gPipeline.mPrismLensRT;

    LLViewerCamera& camera = LLViewerCamera::instance();
    // Keep origin and axes. Install crop projection and recompute frustum planes.
    set_current_projection(mPrismProjection);
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.loadMatrix(glm::value_ptr(mPrismProjection));
    glViewport(0, 0, mTargetWidth, mTargetHeight);
    LLViewerCamera::updateFrustumPlanes(camera, false, false, true);

    camera.setUserClipPlane(mLensPlaneAgent); // coarse cull

    static LLCullResult lens_cull;
    lens_cull.clear();
    gPipeline.updateCull(camera, lens_cull);
    gPipeline.stateSort(camera, lens_cull);

    auto& rt = gPipeline.mPrismLensRT;
    rt.deferredScreen.bindTarget();
    rt.deferredScreen.clear();
    gPipeline.renderGeomDeferred(camera, false);
    rt.deferredScreen.flush();

    gPipeline.renderDeferredLighting(); // writes linear beauty into rt.screen
    // Do not call renderFinalize(): the main view will post-process once.
}
```

`ScopedPrismRenderState` is not optional. It must restore:

- `LLViewerCamera` value, projection/modelview globals, GL matrices, and viewport.
- `gPipeline.mRT`.
- `LLViewerCamera::sCurCameraID` (prefer adding `CAMERA_PRISM_LENS` before `NUM_CAMERAS`).
- `sUseOcclusion`, `sUnderWaterRender`, render-type masks, clip-plane state, bound target/shader, and canonical blend/depth/color masks.
- The main velocity projection if the prism path touched `setPerspective()`.

Add `!sPrismLensRender` beside existing `!gCubeSnapshot` guards for main-only side effects, especially velocity rendering and last-frame matrix history at `pipeline.cpp:15783-15813`. Do not set `gCubeSnapshot` as a shortcut: it chooses probe-specific lighting shaders and suppresses unrelated features with cubemap semantics.

### Behind-lens clip plane

For a planar face center `C`, world normal `N`, and eye `E`, orient the plane so the kept half-space points away from the eye:

```cpp
LLVector3 keep_normal = N;
if ((E - C) * keep_normal > 0.f) // N points toward the eye
    keep_normal = -keep_normal;

// Keep dot(n, X) + d >= 0. A small epsilon also removes the lens surface.
const F32 epsilon = 0.002f;
LLPlane plane(C + keep_normal * epsilon, keep_normal);
```

Use it in camera culling and in the existing mirror-style fragment clipping. The designated face itself must also be skipped while `sPrismLensRender` is true, so it cannot feed back into its own texture. Face-level skipping is preferable; skipping the whole prim is an acceptable MVP fallback.

Khronos specifies user clip half-spaces as non-negative `gl_ClipDistance` values, and depth testing compares the incoming fragment depth to the bound framebuffer depth. The current fragment-discard mirror mechanism provides the same visible half-space rule without adding a vertex output to every material permutation. See the [Khronos clipping description](https://wikis.khronos.org/opengl/Viewport#User-defined_clipping) and [depth-test description](https://wikis.khronos.org/opengl/Depth_Test).

### Composite

The cheapest correct opaque composite is to draw the actual face geometry. No stencil is needed:

- Geometry rasterization is the exact footprint mask.
- `mMainRT.screen` already shares the deferred depth buffer at `pipeline.cpp:1179`.
- `GL_LEQUAL` makes the lens pass only where the face is no farther than the existing opaque depth.
- Depth writes stay off, so the lens is a color replacement, not new scene geometry.
- Scissor to the projected AABB is a harmless optimization.

The composite must happen into the linear `mMainRT.screen` before `renderFinalize()`, so exposure, bloom, AA, and color correction apply once to the integrated frame.

Illustrative GLSL:

```glsl
// prismLensV.glsl
uniform mat4 modelview_projection_matrix;
in vec3 position;
out vec4 prism_clip_pos;

void main()
{
    prism_clip_pos = modelview_projection_matrix * vec4(position, 1.0);
    gl_Position = prism_clip_pos;
}
```


```glsl
// prismLensF.glsl
uniform sampler2D prismLensMap;
uniform vec2 lensRectMinNdc;
uniform vec2 lensRectMaxNdc;
uniform float edgeFeather;
uniform float opacity;

in vec4 prism_clip_pos;
out vec4 frag_color;

void main()
{
    vec2 ndc = prism_clip_pos.xy / prism_clip_pos.w;
    vec2 uv = (ndc - lensRectMinNdc) / (lensRectMaxNdc - lensRectMinNdc);

    // Optional rectangular edge feather. A circular/art mask can multiply alpha.
    vec2 edge = min(uv, 1.0 - uv);
    float a = smoothstep(0.0, edgeFeather, min(edge.x, edge.y)) * opacity;
    vec3 linear_beauty = texture(prismLensMap, clamp(uv, 0.0, 1.0)).rgb;
    frag_color = vec4(linear_beauty, a);
}
```

Illustrative draw state:

```cpp
void LLPrismLensManager::compositeOpaqueCorrect(LLFace& face)
{
    LLRenderTarget& main = gPipeline.mMainRT.screen;
    main.bindTarget();

    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
    LLGLDisable cull(GL_CULL_FACE); // lens may be viewed from either side

    glEnable(GL_SCISSOR_TEST);
    glScissor(mRectX, mRectY, mRectW, mRectH);

    gPrismLensProgram.bind();
    const S32 channel = gPrismLensProgram.enableTexture(LLShaderMgr::PRISM_LENS_MAP);
    gPipeline.mPrismLensRT.screen.bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);

    gGL.pushMatrix();
    LLDrawable* drawable = face.getDrawable();
    if (drawable->isActive())
        gGL.multMatrix((GLfloat*)drawable->getRenderMatrix().mMatrix);
    else
        gGL.multMatrix((GLfloat*)drawable->getRegion()->mRenderMatrix.mMatrix);
    face.renderIndexed();
    gGL.popMatrix();

    gPrismLensProgram.unbind();
    glDisable(GL_SCISSOR_TEST);
    main.flush();
}
```

The transform can follow `LLFace::renderSelected()` at `llface.cpp:511-595`, while indexed drawing is already exposed by `LLFace::renderIndexed()` at `llface.cpp:2232-2238`.

Do not invent a hard-coded texture unit. Add `PRISM_LENS_MAP` and `"prismLensMap"` to the reserved-uniform tables in `llshadermgr.h/.cpp`, then use `enableTexture()`. The shader mapper dynamically assigns reserved samplers at link time (`llglslshader.cpp:772-800`), so the "ReShade/projvol fixed sampler collision" concern is largely a false framing. The real constraint is total sampler count in this particular shader; the composite shader only needs one sampler.

## Answers to the five open questions

### 1. Cheapest correct depth-occlusion composite

**Recommendation: draw the designated face with a special shader, depth test on, depth writes off.**

This beats stencil because it uses one geometry pass instead of a stencil-fill pass plus a composite pass. It beats a shader-side main-depth comparison because fixed-function depth already performs the exact comparison and can early-reject fragments.

Stencil remains useful only if the desired mask is not identical to the face geometry—for example, a circular aperture cut into a square face. Even then, an alpha/art mask in the composite shader is cheaper unless another effect needs the stencil later.

Opaque foreground occlusion is correct immediately. Transparent foreground correctness requires alpha-order integration, discussed below.

### 2. Deferred feature set versus cost

| Feature | MVP behavior | Reason |
|---|---|---|
| Deferred opaque + PBR | Include | Core visual requirement. |
| Avatars/animesh | Include | The main user story. |
| Local lights | Include | Already accumulated by deferred lighting from the lens cull set. |
| Reflection probes | Sample existing probes | No need to update probes again. |
| Sun/spot shadows | Reuse main-view maps | Same eye and a sub-frustum make main cascades a valid superset. Route prism shadow binding to `mMainRT.shadow`; do not allocate/regenerate prism shadows. |
| SSAO | Optional, default on if affordable | Needs lens-sized `deferredLight`; coherent because it is computed from lens G-buffer depth/normals. |
| Alpha-masked geometry | Include | Hair/foliage correctness. |
| Blended alpha | Include in auxiliary view; main composite ordering deferred | Necessary for hair, particles, glass, but main-view transparent occlusion is the hardest remaining issue. |
| Atmosphere/haze | Phase 1.5 | Current helpers use global `mWaterDis`, making resolution/state coupling explicit. Add lens-sized scratch or a lens gate. |
| Water | Phase 1.5 | `renderGeomPostDeferred()` interleaves water/haze and uses global exclusion scratch. Validate separately. |
| SSR | Skip | View-dependent screen-space history/scratch belongs to the auxiliary view and is not worth an MVP rerun. |
| Bloom, exposure, tone map, AA | Main pass once | Composite the linear lens beauty before main finalize. |
| DoF/motion blur | Main pass once, document limitation | They use main depth/velocity and cannot infer the alternate depth inside the lens. Correct lens-local DoF/velocity would need auxiliary metadata and a depth-aware composite. |
| Volumetrics/weather | Skip initially | Several paths are main-view/global-history systems. Add after isolation tests. |

At 960×540, a minimal HDR G-buffer plus beauty/depth/light is on the order of tens of MB, not hundreds. Geometry submission and skinning/draw overhead—not target memory—will usually dominate. A narrow crop cull and one-lens cap are the most valuable controls.

### 3. Same-eye zoom versus offset eye

**Use the same eye point and the cropped off-axis projection.** It matches the requested "different FOV" behavior, retains main-view parallax, and does not reveal surfaces unavailable to the real camera.

An offset camera is useful only as an explicitly named "portal/crystal ball" mode. It is not a physical magnifier approximation.

A later true-optics mode should bend rays around a lens normal/focal model and use object distance. Geometric optics defines magnification through ray/image geometry and angular size; see [MIT OpenCourseWare's magnifier/optics lecture](https://ocw.mit.edu/courses/2-71-optics-spring-2009/resources/mit2_71s09_lec07/) and the [Feynman Lectures' lens magnification derivation](https://www.feynmanlectures.caltech.edu/I_27.html#Ch27-S4). One pinhole rerender cannot express the continuum of refracted rays.

### 4. Exact frame hook

Use two hooks:

1. **Auxiliary render:** after main shadows/impostor updates and before main `stateSort()` in `display()`—approximately the window between `llviewerdisplay.cpp:870` and `920`.
2. **Composite:** inside the main linear-HDR forward/post-deferred phase, before `renderFinalize()`.

For a fast opaque-only spike, composite immediately after `renderDeferredLighting()` returns. This proves target, projection, clipping, color space, and opaque depth behavior, but blended objects in front of the lens will be wrong because they have already rendered and usually did not write depth.

For the robust implementation, integrate a prism draw item into the alpha pool's back-to-front ordering at the lens face's camera depth. Then:

- Transparent surfaces behind the lens draw first and are replaced by the lens composite.
- The lens draws next.
- Transparent surfaces in front draw afterward and correctly overlay it.

This is more invasive than an end-of-pass overlay, but it is the only general solution in the existing forward-alpha model. A depth texture cannot recover ordering for blended fragments that never wrote depth.

### 5. Sampler allocation

Add one reserved sampler enum/name and let `LLGLSLShader` assign its channel. There is no need to coordinate a fixed numeric slot with ReShade or projector volumetrics because those are different shader programs and the mapper assigns channels per program.

Validate at shader-link time that the returned channel is non-negative, and fail the prism shader closed if it is not. The relevant mapper warns that an unreserved sampler defaults incorrectly to unit zero at `llglslshader.cpp:794-800`.

## Transparency, color, and temporal correctness

### Transparent ordering

There are two distinct transparency problems:

1. **Transparency behind the lens** belongs in the auxiliary view and should be clipped at the lens plane.
2. **Transparency in front of the lens** belongs in the main view and must sort after the composite.

The direct face composite solves opaque occlusion, not problem 2. Call the spike "opaque-correct," then land alpha-pool integration before claiming fully correct occlusion.

### Color space

Sample `mPrismLensRT.screen`, not `postPingMap`. It is the linear HDR beauty buffer. Composite into `mMainRT.screen`, then let `renderFinalize()` perform the single main color correction/post chain (`pipeline.cpp:14521-14731`; called from `llviewerdisplay.cpp:1590-1595`).

Avoid compositing a gamma-corrected lens texture into linear HDR; that creates obvious contrast and bloom seams.

### Temporal Capture / Freeze World

No second clock should be advanced. Temporal Capture evaluates presentation state once before rendering; both passes read the same object transforms, animation poses, texture-animation state, and shader time values in the same viewer frame.

The prism pass must be render-only and suppress main-frame history writes. In particular, do not let it:

- Advance `gGLLastModelView` / `gGLLastProjection`.
- Publish ReShade sidecars or frame metadata.
- Write the main velocity target.
- Update probe schedules, occlusion-query history, avatar LOD/impostor history, or per-frame debug counters twice.

This is the real Temporal Capture requirement: **same already-frozen state, no duplicate state mutation**, rather than adding a new presentation-time read to the lens manager.

## Interaction hazards specific to this fork

- **Ghost Studio:** the main ghost G-buffer queue is built after main `stateSort()` at `llviewerdisplay.cpp:1063-1087`, later than the proposed prism auxiliary render. Ghost scene dressing will therefore be absent from the lens unless a lens-view queue is built earlier or the auxiliary hook moves. Treat this as an explicit Phase 2 integration, not an accidental omission.
- **Main temporal matrices:** `renderDeferredLighting()` snapshots last matrices at `pipeline.cpp:15793-15813`. Add a prism exclusion guard.
- **Velocity:** the velocity pass is global and currently gated only by `!gCubeSnapshot` at `pipeline.cpp:15783-15791`. Add `!sPrismLensRender`.
- **Visible-diffuse/ReShade sidecar:** target-identity checks already limit this to `mMainRT` at `pipeline.cpp:6603-6621`, which is good. Keep that invariant.
- **Water/atmosphere scratch:** `mWaterDis` and `mWaterExclusionMask` are global main-sized targets. Do not assume they are prism-safe at a different viewport size.
- **Mirrors and recursive lenses:** do not run prism composite while `sPrismLensRender` is true. A mirror visible in the lens may sample its already-produced hero-probe texture, but the lens must never invoke another lens render recursively. Set a hard recursion depth of one.
- **Selection/edit mode:** designation is local metadata. Never change object material, alpha, flags, or simulator state merely to mark a lens.
- **Object motion between passes:** both passes occur in one frame after geometry update, so transforms should match. Avoid any update/tick call inside the auxiliary renderer.
- **Near-plane/lens intersection:** disable the lens when the eye is on or behind the designated plane, within an epsilon, or when the projected rect crosses the camera plane.

## Suggested settings and UI semantics

- `PrismLensEnabled` — Bool, default false.
- `PrismLensZoom` — F32, default 2.0, clamp 1.0–8.0.
- `PrismLensResolutionScale` — F32, default 1.0, clamp 0.25–2.0, explicitly relative to projected lens pixels.
- `PrismLensMaxLenses` — S32, default 1, hard cap 2 initially.
- `PrismLensIncludeAtmosphere` — Bool, default false until isolated scratch exists.
- `PrismLensDebug` — Bool, default false; shows face id, projected rect, target size, auxiliary CPU/GPU time, and rejection reason.

Director UI:

- **Designate selected face**
- **Clear lens**
- Enabled toggle
- Zoom and resolution-scale sliders
- Read-only object name / UUID / face index / current target size

Reject ambiguous multi-object or multi-face selection with a notification. Do not silently choose the first face.

## Phased implementation and acceptance tests

### Phase 0 — projection/target spike (1–2 days)

- Add minimal prism target pack and scoped state restore.
- Render cropped off-axis view with fixed debug rect.
- Blit full-screen for inspection.
- Confirm no `AgentFOV` messages, no main history mutation, and stable GL state.

Acceptance:

- Off-center crop stays aligned while camera pans.
- Zoom 2.0 halves angular source width/height.
- Enable/disable is pixel-identical outside the debug blit and has no next-frame camera jump.

### Phase 1 — designated planar face, opaque-correct (2–5 days)

- Object UUID + TE designation and pruning.
- Lens-plane clipping in auxiliary materials.
- Draw real face into main HDR screen with depth test/write state described above.
- Reuse main shadow maps; skip SSR/volumetrics and gate unsafe water scratch.
- UI/settings/debug timings.

Acceptance scenes:

1. Avatar fully behind lens: magnified and aligned.
2. Opaque pole between camera and lens: pole occludes the lens and never reappears magnified inside it.
3. Object between lens and avatar: appears magnified; object in front of lens does not.
4. Lens at screen edge and rotated in world: crop remains centered/aligned.
5. Freeze World and slow Temporal Capture: main and lens poses match exactly.
6. UI hidden and HUD attachments enabled: HUD never appears in lens.
7. Lens sees mirror and mirror sees lens: no recursion or undefined sampling.

### Phase 1.5 — forward features (1–3 days)

- Isolate lens-sized water/atmosphere scratch or deliberately define unsupported behavior.
- Verify PBR alpha mask, rigged hair, particles, waterline, and underwater camera.
- Add alpha-pool ordering for transparent foreground correctness.

### Phase 2 — polish and scale (2–4 days)

- Up to two lenses rendered serially through one reusable target pack.
- Per-lens update culling, resolution hysteresis, edge/art masks, material tint/reflection overlay.
- Ghost Studio lens-view queue.
- GPU timer queries and an explicit budget warning; never silently reduce main quality.

### Phase 3 — physical refraction research

Treat this as a separate renderer:

- Per-fragment refracted rays based on surface normal, index of refraction, and focal model.
- A method to resolve scene color/depth along bent rays (ray tracing, layered/cubemap captures, or approximated screen-space tracing).
- Defined behavior for occluded/off-screen samples and multiple depth layers.

Do not describe an offset pinhole camera as this mode.

## Final recommendation

Proceed with Tier 1, but revise the implementation brief before coding:

1. Replace the `mWaterDis` precedent with `RenderTargetPack`/reflection-probe hot swapping.
2. Replace "private camera is enough" with a scoped singleton/global-matrix auxiliary render or a larger camera-parameter refactor.
3. Add a mandatory behind-lens clip plane.
4. Replace symmetric `baseFOV / zoom` with a projected-rect crop/off-axis projection.
5. Composite by drawing the real face into shared-depth `mMainRT.screen`.
6. State that Phase 1 is opaque-correct until alpha-pool ordering lands.
7. Reuse main shadows/probes, post-process once, and gate all main-only temporal/history side effects.

That path is both cheaper and more correct than the original sketch, while staying close to machinery the fork already exercises in production.

## Primary source anchors

Local fork anchors (all under `I:\alchemy-machinima\indra`):

- `newview/pipeline.h:893-928` — render-target packs and active-pack pointer.
- `newview/pipeline.cpp:1081-1111` — recursive auxiliary/hero-pack allocation.
- `newview/pipeline.cpp:1135-1184` — deferred targets and shared depth.
- `newview/pipeline.cpp:1283-1292` — actual `mWaterDis` / `mSceneMap` purpose and allocation.
- `newview/llreflectionmapmanager.cpp:764-798` — auxiliary target-pack swap.
- `newview/llviewerwindow.cpp:6042-6194` — scoped alternate camera render and restore.
- `newview/llviewerdisplay.cpp:1247-1357` — simplified alternate-view deferred frame.
- `newview/llviewercamera.cpp:295-425` — projection, `glm::pickMatrix`, and frustum update.
- `newview/llviewercamera.cpp:801-832` — broadcasting versus no-broadcast FOV setters.
- `newview/pipeline.cpp:14860-15131` — deferred texture/shadow binding and singleton camera dependencies.
- `newview/pipeline.cpp:15151-15815` — deferred lighting, post-deferred geometry, and main history writes.
- `newview/llface.cpp:511-595, 2232-2238` — face transform/draw precedents.
- `llrender/llglslshader.cpp:772-800` — reserved-sampler channel mapping.

External primary/authoritative references:

- [Khronos OpenGL depth test](https://wikis.khronos.org/opengl/Depth_Test)
- [Khronos framebuffer objects](https://wikis.khronos.org/opengl/Framebuffer_Objects)
- [Khronos user-defined clipping](https://wikis.khronos.org/opengl/Viewport#User-defined_clipping)
- [OpenGL 4.6 core specification index](https://registry.khronos.org/OpenGL/specs/gl/)
- [MIT OpenCourseWare: mirrors, magnifiers, and microscopes](https://ocw.mit.edu/courses/2-71-optics-spring-2009/resources/mit2_71s09_lec07/)
- [Feynman Lectures: geometrical optics and magnification](https://www.feynmanlectures.caltech.edu/I_27.html#Ch27-S4)
