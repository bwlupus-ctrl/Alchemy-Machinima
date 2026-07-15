# [BDMerge A5.4] Motion-Vector / Velocity-Buffer Subsystem — Implementation Brief

Status: **Phase 1a + 1c + 1b IMPLEMENTED** (`develop`). 1a = rigid + camera velocity;
1c = fullscreen camera-motion fallback; 1b (2026-07-15) = rigged/skinned + classic-avatar
velocity. Phases 2 (SMAA T2x reprojection) and 3 (motion blur) remain PLANNED. This document
is the executable, phased plan for porting Black Dragon's velocity-buffer subsystem into the
Alchemy-Machinima fork.

## Phase 1b — status: DONE (2026-07-15)

Shipped (same gate: `BDMergeVelocityBuffer` off = velocity pass never runs = byte-identical):
- `MatrixPaletteCache` += `mLastGLMp`/`mLastFrame` (`llvoavatar.h`); swap-stash of the outgoing
  palette in `updateSkinInfoMatrixPalette` (`llvoavatar.cpp`) — allocation-free steady-state.
- New reserved uniform `AVATAR_LAST_MATRIX` ("lastMatrixPalette"), lockstep in `llshadermgr.h/.cpp`.
- `getLastObjectSkinnedTransform()` + `lastMatrixPalette[MAX_JOINTS_PER_MESH_OBJECT]` added to
  `avatar/objectSkinV.glsl` (BD-style: dead-code-eliminated in every non-velocity rigged shader).
- `LLRenderPass::uploadVelocityMatrixPalettes` (`lldrawpool.cpp`) uploads BOTH palettes per
  (avatar, mesh) key. **Hardened over the donor:** falls back to the CURRENT palette (zero limb
  velocity, camera velocity intact) when the previous palette is non-contiguous
  (`mLastFrame != gFrameCount-1` — culled/just-appeared avatar, pitfalls 2/3) or joint counts
  changed; donor left the previous avatar's palette in the uniform (one-frame velocity spike).
- `pushRiggedVelocityBatches`/`pushRiggedVelocityBatchesTextured` + rigged wiring in ALL pool
  velocity hooks: Simple, Fullbright, AlphaMask, FullbrightAlphaMask (`lldrawpoolsimple.cpp`),
  Bump/Shiny/FullbrightShiny (`lldrawpoolbump.cpp`), all 12 material sub-passes + 1
  (`lldrawpoolmaterials.cpp`), GLTF PBR `mRenderType + 1` (`lldrawpoolpbropaque.cpp`).
  Each rebinds the `make_rigged_variant` program and re-uploads bindVelocityUniforms
  (distinct GL program, own uniform storage).
- Programs: `gVelocitySkinnedProgram`, `gVelocityAlphaSkinnedProgram` (make_rigged_variant),
  `gAvatarVelocityProgram` (classic; `avatarVelocityV.glsl` NEW + reuses `velocityF.glsl`;
  un-jittered convention, row-vector `pos * skin` palette apply verified against `avatarV.glsl`).
- Classic-avatar prev palette: `LLViewerJointMesh::mLastMatrixPalette[45*4]`/`mLastMatrixPaletteFrame`
  (U32-underflow-safe staleness reseed), uploaded in `uploadJointMatrices` when
  `gAvatarVelocityProgram` is bound; avatar pool velocity hooks in `lldrawpoolavatar.cpp`
  (impostor / too-slow / invisible guarded — camera fallback covers those).

Owed in-world validation (BDMergeVelocityBuffer + BDMergeVelocityDebug):
1. Animated avatar, camera LOCKED: limbs paint RG color in the debug viz (pre-1b they read
   flat grey = camera-only). Worn rigged hair/clothing moves with the body.
2. Avatar walking past a static camera: whole-body coherent color, no trail.
3. Impostored crowd: no velocity garbage (should read camera fallback grey).
4. Toggle BDMergeVelocityBuffer off → identical stock frame.

## Phase 1a — status: DONE

Shipped (all gated behind `BDMergeVelocityBuffer`, default OFF = byte-identical stock):
- Velocity RT `mVelocityMap` (`GL_RG16F`, shares deferred-screen depth), allocate/release beside
  the SMAA T2x buffers (`pipeline.cpp` screen-buffer block + release path); realloc on toggle
  via `handleReleaseGLBufferChanged` (`llviewercontrol.cpp`).
- `LLPipeline::renderGeomVelocity()` velocity geometry pass + `sVelocityRender` flag; invoked in
  `renderDeferredLighting` after post-deferred geometry, before the last-frame matrix snapshot,
  guarded `!gCubeSnapshot`.
- Four per-pool virtual hooks on `LLDrawPool` (`getNumVelocityPasses`/`begin`/`render`/
  `endVelocityPass`, base = 0 passes) implemented rigid-only for Simple, Grass, Fullbright,
  AlphaMask, FullbrightAlphaMask, Bump (+Shiny/FullbrightShiny), Materials (12 sub-passes),
  GLTF PBR (opaque+alpha-mask), Terrain, Tree. **Rigged pushes are stubbed with explicit
  Phase 1b seam comments.**
- `LLRenderPass::pushVelocityBatches` / `pushVelocityBatchesTextured` (rigid, with per-object
  prev-matrix write-back) + `bindVelocityUniforms` helper (`lldrawpool.cpp`).
- Prev-state plumbing: `LLDrawInfo::mLastModelMatrix` (`llspatialpartition.h`) ←
  `LLDrawable::mLastVelocityMatrix` (`lldrawable.h`), wired in
  `LLVolumeGeometryManager::registerFace` (`llvovolume.cpp`). Camera prev view-proj reuses the
  existing `gGLLastModelView` snapshot.
- Shaders (`class1/deferred/`): `velocityV/F.glsl`, `velocityAlphaV/F.glsl`, `velocityDebugF.glsl`
  (varyings inlined — no separate func file). Programs `gVelocityProgram`,
  `gVelocityAlphaProgram`, `gVelocityDebugProgram` (`llviewershadermgr.*`). New reserved uniform
  enums `CURRENT_MODELVIEW_MATRIX`, `LAST_MODELVIEW_MATRIX`, `LAST_OBJECT_MATRIX`,
  `PROJECTION_MATRIX_UNJITTERED`, `DEFERRED_VELOCITY`, `MOTION_BLUR_STRENGTH` (`llshadermgr.*`).
- **Un-jittered velocity (pitfall 1):** `llviewercamera.cpp` captures the pre-jitter projection
  into `gPipeline.mVelocityProjMat`; the velocity shaders rasterize `gl_Position` with the
  jittered MVP (aligns with jittered colour/depth) but compute the motion vector from the
  un-jittered projection for both current and previous → jitter cancels, no ~0.5px wobble.
- Debug viz `BDMergeVelocityDebug`: `renderVelocityDebug` blits `mVelocityMap` to screen in
  `renderFinalize` (rightward→red, upward→green, static→grey; gain = `BDMergeMotionBlurStrength`).
- Settings: `BDMergeVelocityBuffer` (Bool 0), `BDMergeVelocityDebug` (Bool 0),
  `BDMergeMotionBlurStrength` (S32 32).

Seams left for later phases: rigged/skinned pushes (`push*Rigged*` + `AVATAR_LAST_MATRIX` +
`MatrixPaletteCache.mLastGLMp` — Phase 1b); `HAS_SKIN` branch present but unbuilt in the
velocity shaders; PBR/material alpha-mask over-cover (opaque velocity) — refine if needed;
Phase 2 binds `mVelocityMap` in `resolveSMAAT2x` and additionally allocates it when T2x is on.

Owed in-world validation (no SL session available at implementation time): enable
`BDMergeVelocityBuffer` + `BDMergeVelocityDebug`; pan camera over rigid objects/terrain →
plausible RG velocity (colour shifts with motion direction); hold camera still → near-neutral
grey (~zero); confirm OFF = no extra pass / unchanged image.

## Goal / Why

Our shipped **SMAA T2x** (`[BDMerge A5.8]`) blends the current jittered frame 50/50 with the
previous frame **at the same screen pixel** — no reprojection (`SMAA_REPROJECTION 0`). Any
world or camera motion therefore ghosts, which is the user's complaint.

The fix is a real **screen-space velocity buffer**: a per-pixel motion vector (current NDC −
previous NDC) generated by an extra geometry pass, then consumed by the SMAA resolve to
**reproject** history (`SMAA_REPROJECTION 1`). The same buffer enables **motion blur** as a
bonus (BD's original A5.4 target).

Black Dragon has this subsystem **alive and complete** — it is a faithful map, not a
reconstruction. Our SMAA T2x resolve shader (`SMAAResolveF.glsl` + `SMAA.glsl`) *already*
contains the full reprojection code path behind `#if SMAA_REPROJECTION`; it is dormant only
because no velocity texture is produced. That makes Phase 2 unusually cheap once Phase 1
exists.

Hardware: RTX 5090 / 192 GB. The extra full-screen geometry pass is affordable; perf is not
the gating concern — correctness (rigged palettes, alpha, HUD exclusion) is.

---

## Part A — Black Dragon's subsystem (donor map, `I:\black-dragon`)

### A.1 The velocity render target

- `LLPipeline::mVelocityMap` allocated as **`GL_RG16F`** at screen resolution, sharing the
  deferred screen's depth buffer so the velocity pass depth-tests against the already-rendered
  scene: `pipeline.cpp:1046-1053`
  ```cpp
  if (RenderMotionBlur || RenderFSAAType == 3) {
      mVelocityMap.allocate(resX, resY, GL_RG16F);
      mRT->deferredScreen.shareDepthBuffer(mVelocityMap);
  }
  ```
  (An older RGB/`TT_RECT_TEXTURE` variant is commented out at `pipeline.cpp:987-995`.)
- Released when neither motion blur nor SMAA T2x is active (same block, else branch).
- Note BD already couples the velocity RT to **`RenderFSAAType == 3`** (its SMAA T2x) as well
  as `RenderMotionBlur` — i.e. BD intended velocity to feed T2x reprojection too.

### A.2 The velocity geometry pass

- `LLPipeline::renderGeomMotionBlur()` — `pipeline.cpp:4305-4334`. Binds `mVelocityMap`,
  clears it, sets `sVelocityRender = true`, `LLGLDepthTest(GL_TRUE, GL_FALSE, GL_LEQUAL)`
  (test, no write), then walks **every draw pool** and calls the virtual hook trio:
  ```cpp
  for (pool : mPools) {
      S32 n = pool->getNumMotionBlurPasses();
      for (i in 0..n) { pool->beginMotionBlurPass(i); pool->renderMotionBlur(i); pool->endMotionBlurPass(i); }
  }
  ```
- Invoked from `renderFinalize`-adjacent scene code at `pipeline.cpp:9515-9518`
  (`if (RenderMotionBlur || RenderFSAAType == 3) renderGeomMotionBlur();`), i.e. **after** the
  opaque + post-deferred geometry, **before** the copy of last-frame matrices at
  `pipeline.cpp:9526-9530`.

### A.3 Per-pool virtual hooks

- Declared in `lldrawpool.h:113-116`:
  ```cpp
  virtual void beginMotionBlurPass(S32 pass);
  virtual void endMotionBlurPass(S32 pass);
  virtual S32  getNumMotionBlurPasses();
  virtual void renderMotionBlur(S32 pass = 0);
  ```
- Implemented per pool (each returns 1 pass, binds the right velocity program, uploads
  last/current modelview + viewport, then pushes both rigid and rigged batches). Representative:
  `LLDrawPoolSimple` at `lldrawpoolsimple.cpp:160-192`; alpha-mask/fullbright variants
  through `:318`. Also implemented in: `lldrawpoolbump.cpp:603`, `lldrawpoolmaterials.cpp:301`,
  `lldrawpoolpbropaque.cpp:108`, `lldrawpoolterrain.cpp:218`, `lldrawpooltree.cpp:150`,
  `lldrawpoolalpha.cpp:915-943`, and the avatar pool `lldrawpoolavatar.cpp:500-539`.
- The shared batch pushers live in `lldrawpool.cpp`:
  - `pushVelocityBatches(type)` `:802-836` — rigid: uploads `LAST_OBJECT_MATRIX` from
    `params.mLastModelMatrix` (identity fallback), draws, then **writes current model matrix
    back into `*params.mLastModelMatrix` for next frame** (`:829-834`). This is the
    per-object prev-matrix bookkeeping.
  - `pushVelocityBatchesTextured(type)` `:871-910` — same + diffuse bind (alpha-mask).
  - `pushRiggedVelocityBatches(type)` `:838-869` and `pushRiggedVelocityBatchesTextured`
    `:912+` — rigged: `uploadMatrixPalette(...)` (current) **and**
    `uploadLastMatrixPalette(...)` (previous), then draw.

### A.4 Previous-frame state

Three tiers of "previous" data, each captured differently:

1. **Camera (view-proj):** globals `gGLLastModelView` / `gGLLastProjection`, snapshotted at the
   end of the scene render (`pipeline.cpp:9526-9530`). Uploaded to velocity shaders as
   `LAST_MODELVIEW_MATRIX`, with current as `CURRENT_MODELVIEW_MATRIX`
   (`lldrawpoolsimple.cpp:170-171`).
2. **Rigid per-object model matrix:** `LLDrawInfo::mLastModelMatrix` (a `LLMatrix4*`), declared
   `llspatialpartition.h:104` (`LLMatrix4* mLastModelMatrix = nullptr;`). It points at
   `drawable->mLastVelocityMatrix`, wired in `llvovolume.cpp:5574`
   (`draw_info->mLastModelMatrix = &drawable->mLastVelocityMatrix;`). Read as prev, then
   overwritten with current each velocity draw (A.3).
3. **Rigged/skinned matrix palette (the hard part):** BD extends the avatar palette cache to
   keep the previous frame's GL palette. In `llvoavatar.h:872-892`, `MatrixPaletteCache` gains
   `std::vector<F32> mLastGLMp;` and `S32 mLastFrame`. In
   `LLVOAvatar::updateSkinInfoMatrixPalette()` (`llvoavatar.cpp:10315-10330`), before rebuilding
   this frame's palette it copies last frame's into `mLastGLMp`:
   ```cpp
   if (entry.mFrame != gFrameCount) {
       if (entry.mFrame > 0) { entry.mLastGLMp = entry.mGLMp; entry.mLastFrame = entry.mFrame; }
       entry.mFrame = gFrameCount;
       ... rebuild entry.mMatrixPalette / entry.mGLMp ...
   }
   ```
   `LLRenderPass::uploadLastMatrixPalette()` (`lldrawpool.cpp:951-972`) uploads `mLastGLMp` to
   the `AVATAR_LAST_MATRIX` uniform (`uniformMatrix3x4fv`, mesh rigging) — the mirror of the
   current-palette upload. **This is the elegant BD approach: prev palettes are cached per
   skin-hash keyed by `gFrameCount`, so no per-drawable prev storage is needed.**
   - The classic (non-mesh) avatar joint path has its own copy in
     `llviewerjointmesh.cpp:191-206` (`mLastMatrixPalette[45*4]`, `mLastMatrixPaletteFrame`),
     uploaded to `AVATAR_LAST_MATRIX` for `avatarVelocityV.glsl`.

### A.5 Velocity shaders (donor)

All in `black-dragon/.../shaders/class1/deferred/`:
- `velocityV.glsl` / `velocityF.glsl` — rigid + camera. VS computes
  `pos = MVP * position` (current) and `last_pos = projection * last_modelview * last_object *
  position` (prev); `#ifdef HAS_SKIN` swaps to `getObjectSkinnedTransform()` /
  `getLastObjectSkinnedTransform()`. FS output is the whole subsystem in one line:
  ```glsl
  frag_color = vec4(cur_ndc - last_ndc, 0.0, 1.0);   // NDC-space velocity into RG16F
  ```
  where `cur_ndc = vary_cur_clip.xy / vary_cur_clip.w` etc. (`velocityF.glsl:35-38`).
- `velocityFuncV.glsl` — the `writeVaryVelocity(pos, last_pos)` helper (passes both clip coords
  to the FS).
- `velocityAlphaV/F.glsl` — textured alpha-mask variant (adds texcoord + alpha discard).
- `skinnedVelocityV.glsl` / `skinnedVelocityAlphaV.glsl` — mesh-rig variant. Uses
  `uniform mat3x4 lastMatrixPalette[MAX_JOINTS_PER_MESH_OBJECT]` and a
  `getLastObjectSkinnedTransform()` that blends the prev palette by `weight4`
  (`skinnedVelocityV.glsl:33-71`). Registered as the rigged variant via `make_rigged_variant`
  (`llviewershadermgr.cpp:3193`, `:3207`).
- `avatarVelocityV/F.glsl` — classic-avatar (system-avatar) variant, `lastMatrixPalette[45]`
  `vec4` layout matching `objectSkinV.glsl` (`avatarVelocityV.glsl:27,39-51`).
- `motionBlurV.glsl` / `motionBlurF.glsl` — the composite (Phase 3): 32-tap triangle-weighted
  gather along the velocity vector, early-out under 0.5px, clamped to
  `motion_blur_strength` (`motionBlurF.glsl:37-79`).

### A.6 Program registration (donor)

`llviewershadermgr.cpp`: globals `gVelocityProgram` (`:246`), `gVelocityAlphaProgram` (`:248`),
`gAvatarVelocityProgram` (`:250`, `hasSkinning`), `gDeferredMotionBlurProgram` (`:217`); built at
`:3011-3017` (motion blur), `:3187-3220` (velocity family, incl. `make_rigged_variant` for the
skinned variants → `gVelocitySkinnedProgram` / `gVelocityAlphaSkinnedProgram`).

### A.7 The motion-blur composite (donor, Phase 3 reference)

`renderMotionBlurComposite(src, dst)` `pipeline.cpp:8229-8249`: binds `gDeferredMotionBlurProgram`
with `DEFERRED_DIFFUSE=src`, `DEFERRED_VELOCITY=mVelocityMap`, `DEFERRED_SCREEN_RES`,
`MOTION_BLUR_STRENGTH` (`RenderMotionBlurStrength`, default 32); draws the fullscreen triangle.
Called at `pipeline.cpp:8557-8561` (`if (RenderMotionBlur && !gCubeSnapshot)`), after glow-combine.
Debug viz of the velocity buffer at `:8631-8633`.

---

## Part B — Alchemy-Machinima integration points (`I:\alchemy-machinima`)

### B.1 What we ALREADY have (big wins — reduces the port)

- **The SMAA reprojection shader path is present and complete but dormant.**
  `SMAA.glsl` carries the full Jimenez reprojection code behind `#if SMAA_REPROJECTION`:
  `SMAAResolvePS` reprojection branch `SMAA.glsl:1426-1446` (samples `velocityTex`, inverts it,
  weights by `SMAA_REPROJECTION_WEIGHT_SCALE` = 30, `:522`), plus antialiased-velocity fetches
  in the blend-weights pass `:1361-1412`. `SMAA_DECODE_VELOCITY` defaults to `sample.rg`
  (`:551-552`) — i.e. it expects exactly BD's RG velocity encoding. `SMAAResolveF.glsl:42-66`
  already plumbs the `velocityTex` sampler under `#if SMAA_REPROJECTION`. **Phase 2 is mostly a
  registration + bind change, not new shader math.**
- **SMAA T2x scaffolding is live:** `RenderFSAAType == 3`, `mSMAAHistory` target
  (`pipeline.cpp:1076-1082`), jitter (`sT2xJitterEnabled`), resolve `resolveSMAAT2x`
  (`pipeline.cpp:9342-9394`) called from `renderFinalize` (`pipeline.cpp:10464-10478`).
- **Camera prev-view is free:** Alchemy already snapshots `gGLLastModelView` **and**
  `gGLLastProjection` at end of scene render (`pipeline.cpp:11479-11488`) — identical to BD.
  Nothing to add for camera velocity.
- **Avatar palette cache exists:** `MatrixPaletteCache` + `updateSkinInfoMatrixPalette`
  (`llvoavatar.h:843-864`) — same shape as BD but **without** BD's `mLastGLMp`/`mLastFrame`
  fields. Adding them is a small, localized diff (see B.4).
- **A prev-frame view-proj precedent already ships:** the projector-volumetric temporal keeps
  `mProjVolPrevViewProj` (world→clip) and reprojects against it
  (`pipeline.cpp:9843, 9859-9860`). Same pattern we'll lean on conceptually; the velocity pass
  uses the per-vertex approach instead (more accurate for moving objects).
- `LLDrawInfo::mModelMatrix` exists (`llspatialpartition.h:102`).

### B.2 What we DO NOT have (must port)

- No `mVelocityMap` RT; no `renderGeomMotionBlur`; no per-pool `getNumMotionBlurPasses/
  begin/render/endMotionBlurPass`; no `pushVelocityBatches*` / `uploadLastMatrixPalette`
  (grep: no hits for these in `lldrawpool.h` / newview).
- No `LLDrawInfo::mLastModelMatrix` and no `drawable->mLastVelocityMatrix`
  (grep: **zero** hits across newview). Rigid per-object prev-matrix storage is entirely absent.
- No velocity/`gVelocityProgram*` programs; the LLShaderMgr uniform enums BD relies on
  (`LAST_MODELVIEW_MATRIX`, `LAST_OBJECT_MATRIX`, `CURRENT_MODELVIEW_MATRIX`,
  `DEFERRED_VELOCITY`, `AVATAR_LAST_MATRIX`, `MOTION_BLUR_STRENGTH`, `VIEWPORT`) are **not**
  present in `llshadermgr.h` (grep: no hits) — must be added.
- No velocity shaders in our tree (only the SMAA + projvol matches for "velocity").

### B.3 Draw-pool architecture note

Alchemy's pools mirror the modern SL/BD `LLRenderPass::pushX(type)` model over
`LLDrawInfo`/`LLCullResult`, so BD's `pushVelocityBatches*` port cleanly in shape. The one
structural gap is the missing `mLastModelMatrix` plumbing (B.2) — everything else is a
mechanical translation of BD's pool methods.

### B.4 Where the hard prev-palette work goes

The rigged prev-palette is the single hardest item and maps **exactly** onto our existing
`MatrixPaletteCache`. Add to `llvoavatar.h:843-859`:
```cpp
std::vector<F32> mLastGLMp;   // previous frame's GL palette (velocity)
S32              mLastFrame = -1;
```
and in `LLVOAvatar::updateSkinInfoMatrixPalette` (find our equivalent of
`llvoavatar.cpp:10315`), before the rebuild:
```cpp
if (entry.mFrame > 0) { entry.mLastGLMp = entry.mGLMp; entry.mLastFrame = entry.mFrame; }
```
This is self-cleaning (keyed by skin hash + `gFrameCount`) and needs no per-drawable storage.

---

## Part C — Phased plan

### Phase 1 — Velocity buffer + motion-vector pass (the foundation)

Sub-split by risk: **1a rigid + camera** (low risk), **1b rigged/skinned avatars** (the hard
part). Ship 1a first and validate with the debug viz before starting 1b.

**Phase 1a — rigid + camera velocity**

New state / RT:
- `LLPipeline::mVelocityMap` (`GL_RG16F`, screen res), allocate/release beside the SMAA T2x
  buffers (our `pipeline.cpp:1076-1089` block) gated on
  `RenderFSAAType == 3 || BDMergeMotionBlur`. Share the deferred screen depth buffer
  (`shareDepthBuffer`) as BD does.
- `LLDrawInfo::mLastModelMatrix` (`LLMatrix4*`, default null) in `llspatialpartition.h`;
  `LLDrawable::mLastVelocityMatrix` (`LLMatrix4`); wire `draw_info->mLastModelMatrix =
  &drawable->mLastVelocityMatrix;` in our `llvovolume.cpp` face-setup (mirror BD `:5574`).
- LLShaderMgr uniform enums: `LAST_MODELVIEW_MATRIX`, `LAST_OBJECT_MATRIX`,
  `CURRENT_MODELVIEW_MATRIX`, `VIEWPORT`, `DEFERRED_VELOCITY` (+ name-table entries).

New shaders (port verbatim, they need no refactor):
- `velocityV.glsl`, `velocityF.glsl`, `velocityFuncV.glsl`, `velocityAlphaV/F.glsl`.

New programs: `gVelocityProgram`, `gVelocityAlphaProgram` in `llviewershadermgr.cpp`
(rigged variants deferred to 1b — register the rigid variant only, `make_rigged_variant`
call added in 1b).

Pipeline:
- Port `renderGeomMotionBlur()` (our `pipeline.cpp`) and the `sVelocityRender` flag.
- Port `LLRenderPass::pushVelocityBatches` / `pushVelocityBatchesTextured` into
  `lldrawpool.cpp`, including the write-back of the current matrix into `*mLastModelMatrix`.
- Add the `getNumMotionBlurPasses`/`begin/render/endMotionBlurPass` virtuals to `lldrawpool.h`
  (base no-op returning 0) and implement for the rigid opaque/alpha-mask/fullbright/bump/
  materials/pbropaque/terrain/tree pools. **Rigged batch pushes are stubbed in 1a.**
- Call `renderGeomMotionBlur()` after post-deferred geometry, before the last-matrix snapshot
  (our `pipeline.cpp` ~`11479`), gated on `RenderFSAAType == 3 || BDMergeMotionBlur`.

Settings (campaign convention, default off):
- `BDMergeMotionBlur` (BOOL, default 0), `BDMergeMotionBlurStrength` (S32, default 32),
  `BDMergeVelocityDebug` (BOOL, default 0 — blit `mVelocityMap` for validation).

Validation: enable `BDMergeVelocityDebug`, confirm rigid movers + camera pan write plausible
RG vectors, static scene reads ~0.

- **Effort:** M (2–3 focused sessions). Mostly mechanical porting.
- **Risk:** Low–Med. Pitfalls: (1) depth-buffer sharing must match the deferred screen exactly
  or velocity Z-fights; (2) `mLastModelMatrix` write-back must happen **once per drawable per
  frame** — BD notes double-stamp hazards (`lldrawpoolalpha.cpp:915`); (3) velocity pass must
  respect the same cull/double-sided state as the main pass.

**Phase 1b — rigged / skinned avatar velocity (the hard part)**

- Add `mLastGLMp` / `mLastFrame` to `MatrixPaletteCache` and the copy-forward in
  `updateSkinInfoMatrixPalette` (see B.4).
- Port `LLRenderPass::uploadLastMatrixPalette` (`lldrawpool.cpp:951-972`) + `AVATAR_LAST_MATRIX`
  uniform enum.
- Port `skinnedVelocityV.glsl`, `skinnedVelocityAlphaV.glsl`, `avatarVelocityV/F.glsl`; register
  `gAvatarVelocityProgram` and the `make_rigged_variant` skinned variants of the velocity/
  velocity-alpha programs.
- Port `pushRiggedVelocityBatches` / `pushRiggedVelocityBatchesTextured` and the avatar pool's
  motion-blur hooks (`lldrawpoolavatar.cpp:500-539`); if we retain the classic joint path, port
  `llviewerjointmesh.cpp:191-206` prev-palette copy.

- **Effort:** M–L (2–4 sessions). The math is BD-proven; the cost is breadth (mesh rig + classic
  avatar + alpha-rigged) and getting the palette layout/indexing (`mat3x4` vs `vec4[45]`) exact.
- **Risk:** Med–High. Pitfalls: (1) `MAX_JOINTS_PER_MESH_OBJECT` and 3x4 packing must match our
  `objectSkinV.glsl` exactly; (2) a skin whose cache entry is one frame stale yields wrong prev
  → guard on `mLastFrame == gFrameCount - 1`, else emit zero velocity for that draw; (3) newly
  visible avatars have no prev palette (emit zero, don't reproject).

### Phase 2 — Ghost-free SMAA T2x (resolves the user complaint; needs Phase 1a, ideally 1b)

The resolve math already exists (B.1). Work is wiring:
1. Register a **second** SMAA-resolve program variant compiled with `SMAA_REPROJECTION 1`
   (define injected at build, like our other permutations). Select it when `RenderFSAAType == 3`
   and velocity is available; keep the `SMAA_REPROJECTION 0` program as fallback.
2. In `resolveSMAAT2x` (`pipeline.cpp:9342-9394`): when reprojecting, bind `mVelocityMap` to the
   `velocityTex` sampler (add the enable-texture block mirroring the current/previous binds at
   `:9367-9379`). Also bind velocity in the blend-weights pass if we adopt the antialiased-
   velocity path (`SMAA.glsl:1361-1412`) — optional refinement; the resolve-only reprojection
   (`:1426-1446`) already removes the gross ghosting.
3. Neighborhood clamp: `SMAAResolvePS` already weights history by velocity magnitude
   (`SMAA_REPROJECTION_WEIGHT_SCALE`), which suppresses disoccclusion ghosts. Tune the scale via
   an optional `BDMergeSMAAReprojectionWeight` control if needed.

Settings: none required (T2x already has a toggle); optionally
`BDMergeSMAAReprojection` (BOOL default 1 when T2x on) to A/B the old 50/50 path.

- **Effort:** S–M (1–2 sessions). No new shader math; registration + one texture bind + tuning.
- **Risk:** Med. Pitfalls below (C-pitfalls) — chiefly the **jitter/velocity interaction**.

### Phase 3 — Motion blur (bonus, optional; needs Phase 1)

- Port `motionBlurV/F.glsl` + `gDeferredMotionBlurProgram` + `renderMotionBlurComposite`
  (`pipeline.cpp:8229-8249`), call it in our post chain (analogous to BD `:8557-8561`), after
  glow-combine and **before** UI/HUD, gated on `BDMergeMotionBlur`.
- Uniforms: `DEFERRED_DIFFUSE`, `DEFERRED_VELOCITY`, `DEFERRED_SCREEN_RES`,
  `MOTION_BLUR_STRENGTH` (`BDMergeMotionBlurStrength`).

- **Effort:** S (≤1 session once Phase 1 lands).
- **Risk:** Low. Cosmetic; the early-out + clamp make it safe. Main care: exclude HUD/UI.

---

## Part D — Correctness pitfalls (cross-phase)

1. **Jitter ↔ velocity (Phase 2, important).** Our T2x jitter is baked into the projection at
   `llviewercamera.cpp:396-397` (`proj_mat[2][0/1] += jx/jy`), so `gGLModelView`/`gGLProjection`
   used by the velocity pass are **jittered**. Since the jitter alternates ±0.25px, a velocity
   computed from jittered current − jittered previous carries a spurious ~0.5px oscillation.
   For motion blur it's negligible; for reprojection it injects sub-pixel error into history
   fetch. **Recommended:** compute velocity from **un-jittered** matrices — capture an unjittered
   projection (and its prev) alongside the jittered one, feed those to the velocity program.
   Simplest: store the jitter-free proj in a global at camera-setup time and upload it as the
   velocity pass's `projection_matrix`/`CURRENT_MODELVIEW_MATRIX`. Alternatively accept the
   sub-pixel error initially and revisit if ghosting persists.
2. **Transparent / blend alpha.** BD generates velocity for alpha-*mask* pools (opaque cutout)
   but blended alpha writes are order-dependent; BD renders velocity from a single instance to
   avoid double-stamping `mLastModelMatrix` (`lldrawpoolalpha.cpp:915`). Keep blended-alpha out
   of the velocity pass in 1a; if needed later, stamp velocity from the frontmost layer only.
3. **HUD / UI / nametags exclusion.** The velocity pass runs before UI, and our jitter is
   already disabled for the UI path (`llviewerdisplay.cpp:1510-1513`). Ensure motion blur
   composite (Phase 3) runs **before** UI compositing so the HUD is never blurred/reprojected.
4. **Cube snapshots / reflection probes / selection.** Guard all new passes with
   `!gCubeSnapshot` (BD does) and never jitter/velocity for `for_selection`
   (`llviewercamera.cpp:387`). Probe renders must skip the velocity pass.
5. **First-frame / disocclusion.** New drawables (null `mLastModelMatrix`) and new avatars
   (no `mLastGLMp`) must emit **zero** velocity, not garbage — BD's identity/empty fallbacks
   (`lldrawpool.cpp:823`, `:961-964`) handle this; preserve them. History-invalid frames already
   force blend 0 in our T2x resolve fallback (`pipeline.cpp:9350-9357`).
6. **Perf of the extra geometry pass.** Full re-raster of opaque geometry into RG16F. On the
   5090 this is cheap, but it doubles opaque draw submission — reuse the existing cull results
   (`beginRenderMap`/`endRenderMap`) exactly as BD does; do not re-cull.

---

## Part E — What does NOT port cleanly

- BD wires velocity allocation directly to `RenderFSAAType == 3` and `RenderMotionBlur`
  (`pipeline.cpp:1046`, `9515`). Our AA path is more refactored (`applySMAA`/`resolveSMAAT2x`
  split, `renderFinalize` at `pipeline.cpp:10453-10478`); slot the velocity pass and the
  reprojection bind into our structure rather than copying BD's call sites verbatim.
- BD's `RenderMotionBlur*` settings should be **renamed to the `BDMerge*` convention**
  (`BDMergeMotionBlur`, `BDMergeMotionBlurStrength`) to match this fork's campaign, not carried
  as `RenderMotionBlur`.
- The classic system-avatar joint path (`llviewerjointmesh.cpp`) may be vestigial in our tree;
  confirm whether we still render classic avatars before porting `avatarVelocity*` + the
  `mLastMatrixPalette[45*4]` copy. Mesh-rig (`skinnedVelocity*`) is the path that matters.
- BD's older RECT/RGB velocity map variant (`pipeline.cpp:987-995`) is dead — ignore; use the
  `GL_RG16F` path only (which also matches `SMAA_DECODE_VELOCITY`'s `.rg`).

---

## Part F — Recommended build order & conventions

1. **Phase 1a** (rigid + camera velocity + `mVelocityMap` + debug viz) → validate with
   `BDMergeVelocityDebug`.
2. **Phase 1b** (rigged/skinned prev-palettes) → validate avatar motion in the debug viz.
3. **Phase 2** (SMAA T2x reprojection) → **this resolves the user's ghosting complaint.**
   Phase 1a alone already de-ghosts camera motion and rigid movers; 1b removes avatar ghosting.
4. **Phase 3** (motion blur) — optional, ship when desired.

Conventions: all new controls default **off** (or reprojection auto-on only when T2x is on),
`[BDMerge A5.4]` tags in code comments and commit subjects, patch-log entry in
`doc/BD_MERGE_PATCHLOG.md`, mirror the existing `BDMerge*` settings.xml block style
(`settings.xml:10455+`).

## Anchor quick-reference

| Concern | Black Dragon (donor) | Alchemy (target) |
|---|---|---|
| Velocity RT alloc | `pipeline.cpp:1046-1053` | new, beside `pipeline.cpp:1076-1089` |
| Velocity geom pass | `pipeline.cpp:4305-4334` | new `renderGeomMotionBlur` |
| Pass invocation | `pipeline.cpp:9515-9518` | new, ~`pipeline.cpp:11479` |
| Pool hooks (decl) | `lldrawpool.h:113-116` | new in `lldrawpool.h` |
| Rigid batch push | `lldrawpool.cpp:802-910` | new in `lldrawpool.cpp` |
| Rigged batch push | `lldrawpool.cpp:838-869, 912+` | new (Phase 1b) |
| Prev rigid matrix | `llspatialpartition.h:104`, `llvovolume.cpp:5574` | **absent — add** |
| Prev palette cache | `llvoavatar.h:872-892`, `llvoavatar.cpp:10315-10330` | extend `llvoavatar.h:843-864` |
| uploadLastMatrixPalette | `lldrawpool.cpp:951-972` | new (Phase 1b) |
| Camera prev view-proj | `pipeline.cpp:9526-9530` | **present** `pipeline.cpp:11479-11488` |
| Velocity shaders | `.../deferred/velocity*.glsl` etc. | port verbatim |
| SMAA reprojection math | — | **present, dormant** `SMAA.glsl:1426-1446`, `SMAAResolveF.glsl:42-66` |
| T2x resolve fn | — | `pipeline.cpp:9342-9394` (add velocity bind) |
| Jitter (pitfall) | — | `llviewercamera.cpp:381-398` |
| Motion blur composite | `pipeline.cpp:8229-8249, 8557-8561` | new (Phase 3) |
