# Scene-Lit Clone — P1 Deferred-Draw Recipe (Codex, 2026-07-22)

Concrete recipe for the first scene-lit cut: draw the clone's RIGGED OPAQUE+MASKED batches into the
deferred G-buffer so clones receive real lighting. Behind a default-off toggle. Source: Codex
implementation consult. **Correctness-first order (user directive 2026-07-22): build the validation
harness (invariant checker + contamination test) BEFORE/around this draw, not a toggle-and-eyeball.**

## ⚠️ TOP RISK — the `applyModelMatrix()` trap
The stock single-batch helpers `LLRenderPass::pushBatch/pushGLTFBatch/pushGLTFBatchIndexed` all call
`applyModelMatrix(di)` ([lldrawpool.cpp:724](../indra/newview/lldrawpool.cpp)) which **reloads
`gGLModelView` from `di.mModelMatrix`, destroying the clone's `T(foot)*R*S*T(-pivot)`**. So do NOT use
them unchanged, and do NOT use the collection helpers `pushRiggedBatches*` (they walk gPipeline's
render map, not our harvested vector). Create narrow copies that keep everything EXCEPT `applyModelMatrix`:
```
static void pushGhostBatch(LLDrawInfo& di, bool batch_textures);
static void pushGhostGLTFBatch(LLDrawInfo& di, LLFetchedGLTFMaterial*& last_mat, LLViewerTexture*& last_tex);
static void pushGhostGLTFBatchIndexed(LLDrawInfo& di, LLRenderPass::eGLTFIndexedMaps maps);
```
Keep: texture bindings, texture-matrix setup/teardown, GLTF factor/transform uniforms, indexed slot
arrays, double-sided cull scope, VB `setBuffer()` + `drawRange()`. Remove ONLY `applyModelMatrix(di)`.

## Insertion + entry point
`LLPipeline::renderGhostDeferredOpaqueMasked(const LLCamera&)` called right after
`gPipeline.renderGeomDeferred(camera,true)`, inside deferredScreen, BEFORE texture cleanup /
`deferredScreen.flush()` / SSAO/SSR/velocity / `renderDeferredLighting()`.
```cpp
void LLPipeline::renderGhostDeferredOpaqueMasked(const LLCamera& camera) {
    static LLCachedControl<bool> enabled(gSavedSettings, "GhostDeferredEnable", false);
    if (!enabled) return;   // OFF issues NO GL op -> stock path byte-identical
    // validate queue (getGhostDeferredQueue), establish state, draw
}
```

## First-light whitelist (smallest visible cut) + shaders
| Pass | Deferred shader |
|---|---|
| `PASS_SIMPLE_RIGGED` | `gDeferredDiffuseProgram.bind(true)` |
| `PASS_ALPHA_MASK_RIGGED` | `gDeferredDiffuseAlphaMaskProgram.bind(true)` |
| `PASS_GLTF_PBR_RIGGED` | `gDeferredPBROpaqueProgram.bind(true)` |
| `PASS_GLTF_PBR_ALPHA_MASK_RIGGED` | same PBR shader; cutoff from material |
| (later) legacy material passes | `gDeferredMaterialProgram[idx].mRiggedVariant` + `gPipeline.bindDeferredShader(*shader)` (matches `LLDrawPoolMaterials::beginDeferredPass`) |
| (later) multi-material PBR | `gDeferredPBROpaqueIndexedProgram.bind(true)` |

EXCLUDE first cut: all `*_BLEND_RIGGED`, `PASS_ALPHA_RIGGED`, glow/emissive dups, invisible/post-bump,
fullbright/shiny/bump, legacy `*_EMISSIVE_RIGGED`, static faces. Add after the G-buffer path is stable.

Legacy material single-batch driver (later) uploads `ENVIRONMENT_INTENSITY<-mEnvIntensity`,
`EMISSIVE_BRIGHTNESS<-mFullbright?1:0`, `MINIMUM_ALPHA<-mAlphaMaskCutoff`, `SPECULAR_COLOR<-mSpecColor`;
binds `DIFFUSE_MAP<-mTexture`, `SPECULAR_MAP<-mSpecularMap`, `BUMP_MAP<-mNormalMap`.

## Skin palette (per LLDrawInfo, after shader bind, before draw)
First cut: `if (!LLRenderPass::uploadMatrixPalette(*di)) continue;` (upload every batch — safe).
Optimize later with the dedup overload (`lastAvatar/lastMeshId/lastAvatarShader/skipLastSkin`) —
**reset the cache keys whenever the bound shader changes** (palette uniforms are per-program). Deferred
rigged variants consume the same `AVATAR_MATRIX` uniform as the overlay (`HAS_SKIN` via `mRiggedVariant`).
Frozen clones stay rejected by the queue in this milestone (frozen-palette support = later).

## Clone transform + state scope (the risk)
Per-clone `ScopedGhostTransform` (matches drawGeometryGhost): `MM_MODELVIEW; pushMatrix;
translatef(foot); if(!rot.isIdentity()) multMatrix(LLMatrix4(rot)); if(scale!=1) scalef; translatef(-pivot);
gGLLastMatrix=nullptr; syncMatrices();` — dtor: `popMatrix; gGLLastMatrix=nullptr; syncMatrices();`.
**Never call `applyModelMatrix()` inside.** Once before the clone loop, defensively
`MM_MODELVIEW; gGLLastMatrix=nullptr; loadMatrix(gGLModelView);`.

Outer state scope (minimal): `LLGLDepthTest depth(GL_TRUE, GL_LEQUAL, GL_TRUE); LLGLDisable blend(GL_BLEND);
LLGLEnable cull(GL_CULL_FACE);` + **save incoming color mask** (renderGeomDeferred exits with
`setColorMask(true,false)`), set `setColorMask(true,true)` for the draws, **restore at exit**;
`LLGLEnable srgb(GL_FRAMEBUFFER_SRGB)` around PBR; GLTF driver locally disables cull for double-sided;
always tear down texture matrices. END with: `LLVertexBuffer::unbind(); LLGLSLShader::unbind();
MM_MODELVIEW; gGLLastMatrix=nullptr; loadMatrix(gGLModelView); syncMatrices();`. **Do NOT touch** stencil,
scissor, viewport, FBO, projection, pipeline flags, render targets; no flush/clear/target-switch/lighting/
pool-render/nested renderGeomDeferred/shadow render inside.

## Toggle + overlay double-draw
New `GhostDeferredEnable` (Bool, 0). When ON, the overlay must not redraw a clone that was deferred-drawn
— but do NOT globally `return` from `renderStudioGhosts()` (that wrongly hides frozen/culled/palette-miss/
non-CLONE-style/failed clones). Track per-instance success: `U32 mGhostDeferredSubmittedFrame;
std::unordered_set<LLUUID> mGhostDeferredSubmittedInstances;` — insert `proxy.mInstanceId` only after the
first successful deferred draw. In renderStudioGhosts, `continue` for an item iff
`enabled && style==CLONE && wasGhostDeferredSubmittedThisFrame(item.mInstanceId)`. Accepted limitation:
static/blended parts of a successfully-deferred clone disappear (better than double-drawing them fullbright).

## Accepted first-cut artifacts
Receives lighting but casts NO shadow (shadow maps already rendered); missing static/system-body faces,
blended hair/glass/eyelashes/sheer, fullbright/glow/shiny/bump-only/legacy-emissive; excluded parts hard-
disappear (overlay suppressed); masked edges may be imperfect; PBR indexed/multi-material absent until the
indexed driver is copied; MV/SSR/SSAO may lag (clone inserted after normal traversal).

## Blunt assessment
Safe enough for an in-world TOGGLE test IF: OFF returns before any GL op; `applyModelMatrix()` is never
called inside the clone transform; and the shader/VB unbind + modelview/color-mask restore run at exit.
Most likely catastrophic failure = a modelview or texture-matrix LEAK; most likely non-catastrophic =
wrong material binding; the palette path is comparatively low risk.

## Correctness-first build order (user directive)
1. **Invariant checker** (`LLScopedGhostRenderInvariant`, per SCENE_LIT_CLONE_P0_IMPL_BLUEPRINT.md) +
   inert entry point, wrapped at the call site → proves the entry point is inert (no state leak) BEFORE
   any draw. 2. **P1 draw** (this recipe) wrapped by that checker + behind the toggle → every frame the
   checker VERDICTS whether the draw leaked modelview/color-mask/etc. 3. **G-buffer contamination test**
   (OFF-vs-ON pixel diff) as the stronger proof. The toggle is a safety net ON TOP of the proof, not a
   substitute for it.
