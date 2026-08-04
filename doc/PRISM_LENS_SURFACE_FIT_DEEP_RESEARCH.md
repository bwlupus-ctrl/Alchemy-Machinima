# Prism Lens — surface-fit / "fill the prim face" (deep-research brief for GPT)

> **2026-08-03 implementation status:** Model A is now implemented in the working
> tree. The capped-three-lens addendum at the end of this document supersedes the
> original single-lens scope statements; the surface-fit math and safety rules
> remain normative.
>
> **2026-08-04 extension:** the registry now supports three capture producers
> shared across Surface Lens and Camera Feed, plus sixteen display bindings. See
> `PRISM_VIRTUAL_CAMERA_SURFACE_DEEP_RESEARCH.md` for the normative camera-feed,
> fan-out, cadence, manager, and performance contract.

Codebase: `I:\alchemy-machinima` (branch `develop`). **Historical starting
point:** Slice 1 used one designated face and screen-AABB sampling; the original
brief below explains why that failed to stay surface-locked. Model A and the
capped-three addendum are now implemented, so descriptions of that mapping as
“current” or “remaining” are retained only as design provenance.

---

## Implementation ground rules (build/deploy steps not run in this task)
- **Shaders compile at RUNTIME**, not at build — a broken shader/permutation only fails in-world; verify `#ifdef/#else/#endif` balance by hand across all permutations.
- **Later authorized build command (not run here):** `cmake --build
  "I:\alchemy-machinima\build-Windows-vs2026-os" --config Release --target
  alchemy-bin`. A trailing `MSB3073` (vcpkg `z-applocal` / MochaPro manifest)
  failure after a successful link is benign—verify the Release executable
  timestamp. The viewer must be closed (link lock). New `.glsl` files are not
  copied by `viewer_manifest`; later runtime validation must deploy them to the
  Release shader tree.
- **Windows file open:** `LLFile::fopen(path, LLFILE_MODE("rb"))`, never a bare `"rb"` (`fopen_flags_t` is `wchar_t`).
- **HARD RULES (must stay intact — Slice 1 was reviewed to 0 must-fix on these):**
  1. NETWORK SAFETY: the aux path may call ONLY `LLViewerCamera::setViewNoBroadcast(...)`, NEVER `setView(...)` (it sends `AgentFOV` = a per-frame network flood).
  2. SCOPED RESTORE: the aux pass installs a `ScopedPrismRenderState` RAII that saves/restores the camera singleton, all GL matrices, `gGLViewport`, active `RenderTargetPack`, scissor, color mask, blend, clip-uniform cache, etc. Every new state mutation MUST be inside that scope and restored on every exit path. Output is published (`markProduced()`) only after the scope closes.
  3. BEHIND-LENS CLIP PLANE: a plane a few mm behind the face clips camera-side geometry out of the aux (via `setUserClipPlane` for cull + the `mirrorClip()`/`CLIP_PLANE` fragment path) so foreground can't be re-rendered magnified. Keep it.
  4. HDR ORDER: composite samples the matching retained LINEAR-HDR target
     (`mPrismLensOutput[slot]`, RGBA16F) and runs BEFORE the main screen flush
     and before `renderFinalize()` tonemap/bloom. Never sample a tonemapped target.
  5. DO NOT touch clone/overlay/`actorghostF.glsl`/`LLActorMover` (Ghost Studio — unrelated).
  6. Disabled steady state / no designation ⇒ early return, no aux, no
     composite. The enabled-to-disabled transition releases Prism targets once.
- Current scope is **up to three capture producers and sixteen display bindings,
  opaque-correct**. Transparency and physical refraction remain outside this
  slice.

---

## Historical Slice 1 implementation (superseded by Model A)

The lens is a single-session object in `indra/newview/llprismlens.cpp` (+ `.h`); the composite draw is in `indra/newview/pipeline.cpp` `renderDeferredLighting()`; two shaders in `app_settings/shaders/class1/deferred/`. Anchors below are approximate — **re-confirm against the tree** (all Prism code is UNCOMMITTED in the working copy).

### (1) Aux render — a SCREEN-AABB crop, magnified (`llprismlens.cpp` `renderAuxiliaryView()`, ~811-1020)
`prepare()` (~318-542) projects the designated face's world vertices with the MAIN camera and takes their **screen-space AABB** (`mLensRect` = left/bottom/width/height in main-viewport pixels). The aux is then rendered by cropping the MAIN projection to that AABB (shrunk by `1/zoom`) via `glm::pickMatrix`, into `mPrismLensRT`:

```cpp
// llprismlens.cpp ~848-853, 983-988  (crop = the face's SCREEN AABB / zoom)
const F32 crop_center_x = mLensRect.mX + mLensRect.mWidth  * 0.5f;
const F32 crop_center_y = mLensRect.mY + mLensRect.mHeight * 0.5f;
const F32 crop_width    = mLensRect.mWidth  / frame.mZoom;
const F32 crop_height   = mLensRect.mHeight / frame.mZoom;
...
const glm::mat4 prism_projection =
    glm::pickMatrix(crop_center, crop_size, viewport) * main_projection; // SCREEN-space crop
```
(The cull frustum is a conservative symmetric bound of this off-axis crop — the 2026-08-03 "1.1" fix; keep that idea but it will need to bound the NEW projection instead, see below.)

### (2) getCompositeState — the AABB again, as NDC + scissor (`llprismlens.cpp` ~927-985)
```cpp
// mRectMinNdc/mRectMaxNdc = the face's screen AABB in NDC; mScissor = same AABB in target px
state.mRectMinNdc[0] = (mLensRect.mX - mMainViewport[0]) * inv_width  * 2 - 1;
state.mRectMinNdc[1] = (mLensRect.mY - mMainViewport[1]) * inv_height * 2 - 1;
state.mRectMaxNdc[0] = state.mRectMinNdc[0] + mLensRect.mWidth  * inv_width  * 2;
state.mRectMaxNdc[1] = state.mRectMinNdc[1] + mLensRect.mHeight * inv_height * 2;
// ... mScissor computed from the same AABB, scaled to screen_target px ...
```

### (3) Composite draw (`pipeline.cpp` `renderDeferredLighting()`, ~16211-16320)
Draws the REAL face (mask) with `gPrismLensProgram`, scissored to the AABB, sampling `mPrismLensRT.screen`:
```cpp
LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);   // opaque foreground occludes
glScissor(prism_state.mScissor[...]);                 // AABB bound
gPrismLensProgram.bind();
mPrismLensRT.screen.bindTexture(0, prism_channel, LLTexUnit::TFO_BILINEAR);
gPrismLensProgram.uniform2fv(sLensRectMinNdc, 1, prism_state.mRectMinNdc);
gPrismLensProgram.uniform2fv(sLensRectMaxNdc, 1, prism_state.mRectMaxNdc);
// pushFaceMatrix() resets to gGLModelView then applies the drawable matrix (the 1.2 mask fix),
// so the drawn triangles are now the true face silhouette:
scoped_composite_restore.pushFaceMatrix(lens_drawable);
prism_state.mFace->renderIndexed();
```

### (4) The two shaders (VERBATIM — the crux of the problem)
`app_settings/shaders/class1/deferred/prismLensV.glsl`:
```glsl
uniform mat4 modelview_projection_matrix;
in vec3 position;
out vec4 prism_clip_pos;
void main()
{
    prism_clip_pos = modelview_projection_matrix * vec4(position, 1.0);
    gl_Position = prism_clip_pos;
}
```
`app_settings/shaders/class1/deferred/prismLensF.glsl`:
```glsl
uniform sampler2D prismLensMap;
uniform vec2 lensRectMinNdc;
uniform vec2 lensRectMaxNdc;
uniform float edgeFeather;
in vec4 prism_clip_pos;
out vec4 frag_color;
void main()
{
    vec2 ndc = prism_clip_pos.xy / prism_clip_pos.w;             // <-- SCREEN position of the fragment
    vec2 uv  = (ndc - lensRectMinNdc) / (lensRectMaxNdc - lensRectMinNdc); // <-- mapped into the screen AABB
    if (edgeFeather < 0.0) { discard; }
    frag_color = texture(prismLensMap, clamp(uv, 0.0, 1.0));     // <-- sample aux by SCREEN uv
}
```

---

## Why Slice 1 was broken (the precise defect)
The fragment's aux UV comes from its **screen NDC** mapped into the face's **screen AABB**. Consequences:
- The magnified image is **screen-aligned**, not locked to the surface — it does not foreshorten or rotate with the face; it behaves like a fixed screen-space window, not a picture ON the prim.
- Because the source (`pickMatrix` crop) and the sampling are both the screen AABB, the image is fundamentally a **screen rectangle**, so it cannot "stretch to the size of the prim face" for a tilted/rotated/rolled face — the face's actual surface parameterization is never used.
- The 2026-08-03 "1.2" fix made `renderIndexed` draw the true silhouette (so the OUTLINE now conforms), but the INTERIOR mapping is still screen-space, so it still does not fill/stretch across the surface.

**Goal:** the magnified render must map onto the prim face by the face's own **surface parameterization** so it fills the face edge-to-edge and stays locked to the surface at any orientation (head-on, tilted, rotated, rolled). The prim face should behave like a "screen"/"portal" showing the magnified view.

---

## Target models (pick one; recommend A)

### Model A — "screen/portal through the face quad" (RECOMMENDED for "fill the prim face")
Render the aux as the view **through the face quad** (not through a screen AABB), and sample it by the face's **surface UV**. Then face-UV `[0,1]²` maps 1:1 to the aux, so the magnified image fills the face exactly at ANY orientation.

Two coupled changes:

**A1. Aux projection = generalized off-axis frustum through the face quad** (replaces the `pickMatrix` screen-AABB crop, `llprismlens.cpp` ~983-988). Use **Kooima's generalized perspective projection** ("Generalized Perspective Projection", Robert Kooima 2009) which, given the eye `pe` and three face corners `pa` (UV origin), `pb` (pa + U edge), `pc` (pa + V edge), builds a projection that maps that quad exactly to the viewport `[-1,1]²`:
```
vr = normalize(pb - pa);  vu = normalize(pc - pa);  vn = normalize(cross(vr, vu));
va = pa - pe;  vb = pb - pe;  vc = pc - pe;
d  = -dot(vn, va);                      // eye-to-plane distance (>0 if eye is in front)
n  = near;  f = far;                    // reuse the main camera's near/far
l  = dot(vr, va) * n / d;  r = dot(vr, vb) * n / d;
b  = dot(vu, va) * n / d;  t = dot(vu, vc) * n / d;
P  = frustum(l, r, b, t, n, f);         // asymmetric/off-axis
M  = mat4(vr, vu, vn) transposed;       // align axes to the quad basis
T  = translate(-pe);
prism_projection = P * M * T;           // world -> aux clip; quad corners -> [-1,1]^2
```
- **Zoom** = shrink the quad about its center by `1/zoom` before building the frustum (use a smaller central sub-quad `pa'..pc'`), so the aux captures a narrower region = magnified. face-UV `[0,1]` still maps to the full aux (the magnified sub-region).
- Keep the eye at the MAIN camera origin (so it's a true "look through the glass" view; parallax is correct as the user moves). Keep `setViewNoBroadcast`-only, the behind-lens clip plane, the scoped restore. NOTE: with a full off-axis matrix you may not need `setViewNoBroadcast` at all for projection (you set the GL projection directly), but the LLViewerCamera singleton still must carry a frustum that BOUNDS this projection for `updateCull` — adapt the 1.1 conservative-bound logic to bound the quad's corner rays instead of the screen-AABB crop.
- Corner ordering MUST follow the face's UV layout (which corner is UV (0,0) vs (1,0) vs (0,1)) so the image is not mirrored/rotated — derive `pa/pb/pc` from the volume face's vertices + their texcoords (see A3).

**A2. Pass + sample by face UV** (the shaders):
- `prismLensV.glsl`: add `in vec2 texcoord0;` and `out vec2 vary_texcoord0;`, set `vary_texcoord0 = texcoord0;`. (Confirm the face VB has `texcoord0` bound for this program; the standard deferred VB layout binds position=0, texcoord0=... — match the existing attribute enum.)
- `prismLensF.glsl`: replace the screen-NDC block with `vec2 uv = vary_texcoord0 * uvScale + uvOffset; frag_color = texture(prismLensMap, clamp(uv, 0.0, 1.0));` where `uvScale/uvOffset` (new uniforms, default 1/0) handle any flip or zoom-in-UV framing. Keep `edgeFeather` uniform. Because the aux was rendered through the quad (A1), `vary_texcoord0` maps directly to the aux — no screen dependence. You can then DROP `lensRectMinNdc/MaxNdc` (or keep them only if you also keep a Model-B path).
- Composite host (`pipeline.cpp` ~16317-16320): stop needing the `lensRectNdc` uniforms; keep drawing the real face (`renderIndexed`) as the mask; the AABB scissor can stay purely as an optimization.

**A3. Which UVs? (key decision).** The face carries TWO UV sources:
- the raw mesh/volume-face texcoords (`LLVolumeFace.mTexCoords`, the geometric `[0,1]` parameterization), and
- the on-screen `texcoord0` in the VB, which BAKES the texture-entry mapping (repeats/offset/rotation the user set on that face).
For a "screen that fills the face", you want the **geometric face parameterization** (so the magnified image fills the face once, ignoring TE repeats). If `texcoord0` includes TE repeats, either (i) build the aux/quad from the raw `mTexCoords` and pass those (not `texcoord0`), or (ii) divide out the TE scale/offset. Decide and document. Also handle faces whose UVs are not a clean `[0,1]²` (sculpts, tortured prims): Slice-1 already restricts to **planar** faces (`prepare()` rejects non-planar), so restrict further to faces whose UV range is a simple rectangle, or normalize by the face's UV min/max.

**A4. Cull frustum** must bound the new quad projection. Reuse the 1.1 approach (`llprismlens.cpp` ~918-971) but compute the bounding symmetric frustum from the four **sub-quad corner rays** (angles of `pa'..pd'` from the eye/forward), not the screen-AABB crop. Fail-open wide on degenerate input, as now.

### Model B — projective texturing ("window into a magnified world")
Keep the aux rendered from the eye; in the fragment, project the face fragment's WORLD position through the aux view-projection (`aux_uv = (auxVP * worldPos).xy / w * 0.5 + 0.5`) and sample. Pass `auxVP` as a uniform and the fragment world pos (add a varying). This looks like a real lens/window and aligns content to the world, but it fills only where world content is behind the face, and needs care to "fill the face". Use if the desired look is a see-through lens rather than a screen. (Model A already gives a correct through-the-glass view because the eye stays at the camera; A is strictly the better default for "fills the face".)

### Model C — naive: keep the screen-AABB aux, just sample by face UV (NOT recommended)
Passing `texcoord0` and sampling the EXISTING screen-AABB aux by it will still distort under tilt, because the mapping from surface UV to the screen-AABB crop is a homography, not identity — the aux content is a screen rectangle, not the face's view. Only a stopgap; do not ship as the fix.

---

## Files / anchors to touch (Model A)
- `indra/newview/llprismlens.cpp` — `prepare()` (capture the face's world-space quad corners + their UV assignment, in addition to / instead of the screen AABB); `renderAuxiliaryView()` ~983-988 (replace `pickMatrix*main_projection` with the Kooima quad projection; ~918-971 adapt the cull bound); `getCompositeState()` ~927-985 (provide `uvScale/uvOffset` + drop/replace `mRectNdc`; keep `mScissor` as an optimization); `.h` (carry the quad corners / UV basis in `PrismFrame`).
- `app_settings/shaders/class1/deferred/prismLensV.glsl` — add `texcoord0` → `vary_texcoord0`.
- `app_settings/shaders/class1/deferred/prismLensF.glsl` — sample by `vary_texcoord0` (+ `uvScale/uvOffset`); remove screen-NDC path.
- `indra/newview/pipeline.cpp` — composite uniform set ~16317-16320 (swap `lensRectNdc` for `uvScale/uvOffset`); keep `pushFaceMatrix` + `renderIndexed` (the 1.2 fix) as the mask.
- Reference for correct face-draw matrices (already applied in the 1.2 fix): `LLRenderPass::applyModelMatrix` (`lldrawpool.cpp:761`) and `LLVolumeGeometryManager::registerFace` matrix selection (`llvovolume.cpp:5567`).

## Acceptance criteria
- The magnified image FILLS the designated prim face edge-to-edge and stays locked to the surface at every orientation: head-on, tilted toward/away, yawed, and rolled. No upright-rectangle-that-ignores-tilt; no black/empty margin inside the face.
- Magnification (`PrismLensZoom`) shrinks the source view (true zoom), still filling the face.
- Opaque foreground still occludes the lens (depth test); no re-magnified foreground (behind-lens clip plane holds).
- ZERO `AgentFOV` messages on enable/move/zoom; no main-view corruption (scoped restore holds); composite still linear-HDR before tonemap.
- Disabled steady-state frames perform no Prism preparation/render/composite;
  the enable-to-disable transition only releases Prism targets. Rigged faces
  remain refused.

## Historical design questions (resolved by Model A + the addendum)
1. Model A (screen/portal, recommended) vs Model B (projective window) — confirm the desired look.
2. UV source: raw geometric `mTexCoords` vs TE-baked `texcoord0` (A3) — which gives "fills the face once" for the user's prims (they set textures/repeats on faces)?
3. Corner ordering / flip so the magnified image is not mirrored or rotated relative to the surface.
4. Zoom as sub-quad shrink (A1) vs UV-space crop (A2 `uvScale`) — pick one, keep it consistent with the cull bound.
5. Non-rectangular / non-`[0,1]²` face UVs (sculpts, tortured prims): normalize by UV min/max, or restrict designation to simple planar quad faces.
6. Whether the off-axis matrix removes the need for `setViewNoBroadcast` on the aux camera (projection set directly) while still feeding a bounding frustum to `updateCull`.

---

## 2026-08-03 addendum — practical multi-instance design (hard limit: 3)

### What “multi-instance” means

The local manager owns three fixed slots keyed by `(object UUID, texture-entry
index)`. Different faces on one prim are distinct. A duplicate add is an
idempotent no-op; a rejected fourth add never evicts an existing lens. Invalid
runtime state removes only its own slot. The `PrismLensEnabled` setting is a
master render gate, not registry ownership: lenses can be added and removed while
rendering is disabled, and registry operations do not rewrite the setting. An
enabled-to-disabled transition releases scratch and retained beauties but keeps
the three local identities; re-enable repopulates them through the scheduler.

The Director exposes the registry explicitly: count, three-row list, add exactly
one selected face, remove highlighted, and clear all. List removal is required
because a temporarily missing object may no longer be selectable in-world.

### Renderer architecture

The implementation uses:

- three lazily allocated, exact-size capture scratch `RenderTargetPack`s;
- matching exact-size Prism water color/depth and exclusion targets;
- three fixed, non-movable `GL_RGBA16F` retained beauty outputs;
- one explicit active-slot index for the fragment clip plane;
- current-frame surface/face preparation for every visible lens;
- at most **one** auxiliary cull/sort/geometry/lighting update per viewer frame;
- a linear-HDR real-face composite for every visible lens that has a retained
  output.

The scheduler first initializes visible slots with no output, then refreshes the
most overdue visible lens. Screen area is only a tie-breaker, so a small lens
cannot starve. Thus one visible lens updates every frame, two alternate, and
three refresh at worst every third frame. Retained output reuse is temporal
throttling, not a claim that the world is unchanged; face geometry and scissor
data are still prepared against the current main camera every frame.

Each update follows this publication boundary:

1. Prepare all current faces from the saved main camera/matrices.
2. Select one overdue slot and its own bucketed logical output extent.
3. Allocate or reuse that capture slot's exact-size scratch/water pack.
4. Enter `ScopedPrismRenderState` and the active-slot guard.
5. Render its complete deferred auxiliary view into its own scratch pack.
6. After deferred lighting flushes scratch, copy/downsample into that slot's
   logical persistent output while still inside the state scope.
7. Close the scope, restoring camera/matrices/viewport/targets/uniform caches.
8. Only then publish the new retained output generation.

The composite resolves each real face again, binds the matching slot output,
uses one matrix-push/pop restore scope per face, and draws before tonemap. Each
retained output has fixed 1024-square capacity; host UVs map the published logical
region to texel centers so bilinear sampling cannot bleed into stale capacity
pixels.

### Performance and memory bounds

This is not projector-cheap. A volumetric projector mainly adds bounded shading
work to already-rendered geometry; a Prism update renders another camera view,
including region traversal, cull, state sort, opaque geometry, and deferred
lighting. The defensible performance statement is therefore:

> Up to three configured/visible lenses, with no more than one additional scene
> render per viewer frame, plus up to three inexpensive real-face composites.

Target dimensions use discrete buckets `64, 128, 256, 512, 1024`, and 1024 is a
hard per-axis cap. The Director exposes the global resolution scale as
“Quality all.” Each capture owns a lazily allocated deferred/water pack whose
physical extent exactly equals that capture's bucketed logical extent. Matching
those extents is required because the deferred shaders sample normalized screen
textures; rendering a smaller logical viewport inside a larger scratch target
would read stale texels. Separate packs prevent alternating mixed-size captures
from reallocating one shared pack. Retained beauties have fixed 1024-square
capacity and publish an independent logical subregion. A capture that transitions
to zero display consumers keeps its definition/slot but releases its owned GPU
targets; rebinding a display regenerates them lazily.

At the 1024-square cap, one exact deferred/water pack is approximately 53–59 MiB
depending on enabled attachments. Three packs are approximately 159–177 MiB;
adding three fixed RGBA16F retained outputs brings the hard target-storage bound
to roughly 183–207 MiB before driver/FBO overhead. Typical use is lower because
packs are lazy and exact-size. The one-update scheduler bounds the dominant CPU
and draw cost; lower resolution alone would not bound cull/state-sort work.

### Adversarial corrections incorporated

- `updateCull()` previously replaced or disabled the user clip plane installed
  by Prism. While `sPrismLensRender` is true it now preserves the caller's
  behind-lens plane, avoiding camera-side traversal/submission as well as keeping
  fragment clipping consistent.
- Fixed slots/targets are used because `LLRenderTarget` owns raw GL names and is
  unsafe to place in an erasable/movable value vector.
- Invalid adds are non-mutating; dead/invalid runtime entries affect one slot;
  offscreen, near-clipped, tiny, or drawable-rebuild states are transient skips.
- The active slot is installed before clip uniforms are dirtied, so every aux
  update reads its own plane.
- The scratch-to-output copy occurs after the HDR scratch flush, inside the
  complete render-state scope; output publication occurs after restoration.
- Multi-composite uses one restore object per face. Reusing the old single
  restore around a loop would push the model-view stack three times and pop once.
- Scratch teardown and all three outputs share the normal screen-buffer teardown
  boundary. Per-slot outputs have independent logical sizes. Output failure
  invalidates only that slot; scratch-resize failure resets the failed requested
  capacity; both use retry backoff so a large failed slot cannot permanently
  starve smaller lenses.
- Debug mode displays the most recently refreshed retained slot, never a capture's
  transient deferred scratch.

### Adversarial acceptance matrix

- 0 lenses: no Prism allocation, preparation, cull, render, or composite.
- Disable after use: render resources are released once; following disabled
  frames do no preparation/render/composite work and retain only registry data.
- Add three; duplicate add is a no-op; fourth add is rejected without eviction.
- Invalid selection, dead object, offscreen/tiny face, and drawable rebuild never
  alter the other slots or the master enable setting.
- Three lenses show three distinct retained views, not three copies of scratch.
- SSAO/shadows on and off; main camera, target stack, viewport, scissor, shader,
  color mask, and matrix depth match their pre-aux values.
- Foreground cull/fragment plane remains active; ordinary opaque depth occlusion
  is independent of lens draw order. Distinct coplanar overlapping lens surfaces
  remain intentionally last-draw ambiguous.
- Camera orbit crosses resolution buckets without per-pixel-size allocation
  churn; remove/teardown releases the correct fixed slot.
- Disable/re-enable preserves registry management and retained identities.
- Zero `AgentFOV` traffic: temporary camera FOV remains
  `setViewNoBroadcast(...)` only.

Transparent-pool ordering remains outside this opaque-correct slice. No build was
run for this implementation task; runtime shader permutations and the matrix/GL
acceptance matrix remain explicit handoff tests.
