# Scene-Lit Clone — Solid Completion Rescue Blueprint (Codex, 2026-07-23)

Fixes the in-world regression after coverage Slice 1 (`529baf45cdd`): the user's ornate wardrobes
(metallic gold armor, iridescent feathers) carry fullbright/fullbright-shiny/bump RIGGED passes.
Slice 1's all-eligible-success rule can't cover those in the G-buffer, blocks `GHOST_COVERAGE_RIGGED_SOLID`,
and the WHOLE clone falls back to the flat fullbright overlay -> "looks non-deferred", gold flat, no
reflections. Root cause CONFIRMED in source (collector harvests these passes; deferred loop can't
index them -> all_solid_ok=false -> catInst rs=0 -> overlay_mask=ALL fallback).

## Source correction (Codex vs my mapping)
`PASS_SHINY_RIGGED` is NOT normally a post-deferred shiny pass. Deferred-world non-fullbright shiny
registers as `PASS_SIMPLE_RIGGED` (deferred simple shader includes shiny) or `PASS_BUMP_RIGGED`.
`PASS_SHINY_RIGGED` in harvested groups is non-deferred/HUD/legacy/stale residue -> NORMALIZE it to
the deferred SIMPLE recipe (`gDeferredDiffuseProgram.mRiggedVariant`), do NOT route through the
fullbright-shiny env shader (would change lighting semantics). Count normalizations for diagnostics.
Only `PASS_FULLBRIGHT_SHINY_RIGGED` uses the env-map shader.

## Corrected stock-path map
| Harvested pass | Stock deferred-world path | Ghost shader |
|---|---|---|
| PASS_BUMP_RIGGED | G-buffer | gDeferredBumpProgram.mRiggedVariant |
| PASS_SHINY_RIGGED (anomaly) | normalize -> deferred simple | gDeferredDiffuseProgram.mRiggedVariant |
| PASS_FULLBRIGHT_RIGGED | post-deferred forward | gDeferredFullbrightProgram.mRiggedVariant |
| PASS_FULLBRIGHT_ALPHA_MASK_RIGGED | post-deferred masked forward | gDeferredFullbrightAlphaMaskProgram.mRiggedVariant |
| PASS_FULLBRIGHT_SHINY_RIGGED | post-deferred forward env/reflection | gDeferredFullbrightShinyProgram.mRiggedVariant |
| PASS_ALPHA_RIGGED | post-deferred forward alpha (later slice) | by draw-info material type |
| PASS_GLTF_GLOW_RIGGED | overlay additive (unchanged) | -- |

## A. Consolidated staged forward dispatcher
ONE shared dispatcher, invoked at stock-equivalent pool boundaries INSIDE renderGeomPostDeferred --
NOT a single monolithic call after it (that draws ghost opaque-forward AFTER world alpha -> a ghost
fullbright surface overwrites already-composited glass/particles even when behind them).
```cpp
enum class EGhostForwardStage {
    FULLBRIGHT_OPAQUE, FULLBRIGHT_SHINY, FULLBRIGHT_MASKED,
    ALPHA_PRE_WATER, ALPHA_POST_WATER, FINALIZE };
void LLPipeline::renderGhostPostDeferred(const LLCamera& camera, EGhostForwardStage stage);
```
Invoke around: POOL_FULLBRIGHT (ordinary fullbright), POOL_BUMP (fullbright shiny),
POOL_FULLBRIGHT_ALPHA_MASK (fullbright mask), pre-water alpha boundary, post-water alpha boundary,
then FINALIZE after the last ghost stage.
**Two-hook compromise acceptable for EARLY TESTING:** ghost forward SOLIDS immediately before stock
renderGeomPostDeferred, ghost BLEND immediately after. Stage-boundary is the correct final design.

### Per-group GL state
- **Fullbright opaque:** depth LEQUAL + writes ON, blend OFF, cull ON. Per batch: palette, diffuse/
  indexed textures, texmatrix (no applyModelMatrix), bind EXPOSURE_MAP, ScopedGhostTransform.
- **Fullbright shiny:** depth LEQUAL + writes ON, blend ON (BT_ALPHA), cull ON. Setup from
  beginFullbrightShiny(): bind exposure map, upload transformed SHINY_ORIGIN; if reflection probes
  enabled `gPipeline.bindReflectionProbes(shader)` else enable+bind sky cube map + ENVIRONMENT_MAP +
  diffuse + setEnvMat(); per-batch diffuse; restore cube-map matrix/probe bindings/units. THIS pass
  restores classic gold/environment reflections.
- **Fullbright alpha-mask:** depth LEQUAL + writes ON, blend OFF, cull ON. setMinimumAlpha(cutoff),
  palette, textures, exposure map (mirror actual shader enable result), draw. Stays RIGGED_SOLID.
- **Rigged blend (later slice):** depth LEQUAL + writes OFF, blend ON, RGB per-LLDrawInfo, alpha
  ZERO/ONE_MINUS_SRC_ALPHA, far-to-near at pre/post-water stage.

## B. Cross-phase RIGGED_SOLID (the load-bearing bookkeeping)
`RIGGED_SOLID` is a FINALIZED category result, not a G-buffer-owned bit. Solid faces now draw across
two phases (G-buffer: simple/material/pbr/bump/shiny-normalized; forward: fullbright/fb-mask/fb-shiny).
```cpp
struct GhostCategoryProgress { U32 mRequired=0, mDrawn=0; bool mClassified=false, mFailed=false, mFinalized=false; };
struct GhostSubmissionProgress {
    GhostCategoryProgress mRiggedSolid, mRiggedBlend;
    U32 mExpectedForwardSolidStages=0, mCompletedForwardSolidStages=0; };
std::map<LLUUID, GhostSubmissionProgress> mGhostSubmissionProgress; U32 mGhostSubmissionProgressFrame=0;
```
- **Classify each harvested batch ONCE at G-buffer time** into: gbuffer-solid / forward-fb-solid /
  forward-fb-shiny / forward-fb-mask / forward-blend / glow / unsupported. Every overlay-solid batch
  ++mRiggedSolid.mRequired. Record which forward stages the instance requires.
- G-buffer success: ++mDrawn. Post-eligibility failure: mFailed=true. Forward-required batch: pending
  (does NOT fail for not drawing in G-buffer). Truly unsupported solid: mFailed=true.
- **Forward phase:** each stage processes only its assigned batches; success ++mDrawn, failure
  mFailed=true; mark stage completed EVEN IF it failed (else finalize can't tell failure from a
  never-run entry point).
- **Finalize** (explicit FINALIZE stage near end of renderGeomPostDeferred, before overlay):
  `complete = classified && !failed && required>0 && drawn==required && all_required_forward_stages_completed`
  -> only then set GHOST_COVERAGE_RIGGED_SOLID. MONOTONIC: never set-in-G-buffer-then-clear.
- Forward pass consumes the SAME validated frame/view queue as the G-buffer pass; do not rebuild.

## C. Bump into the G-buffer
`gDeferredBumpProgram.mRiggedVariant`; enable DIFFUSE_MAP + BUMP_MAP. Per batch: validate VB/count/
avatar/skin; setMinimumAlpha(mAlphaMaskCutoff); `LLDrawPoolBump::bindBumpMap(params, bump_channel)`
(false -> batch failed -> blocks solid coverage); upload palette; bind diffuse; texmatrix; drawRange;
count success only after draw. NEW `pushGhostBumpBatch(params, diffuse_channel)` = pushBumpBatch
([lldrawpoolbump.cpp:1059]) MINUS applyModelMatrix (:1062). Traps: don't reuse file-static
shiny/diffuse_channel/shader-level globals; implement the texmatrix result explicitly (activate
diffuse unit, enter texture matrix, load batch matrix ONCE, draw, restore identity+modelview); mBump
+ source texture selects generated brightness/darkness bump maps (binding only mTexture insufficient);
preserve alpha cutoff; disable both channels + restore active unit at exit. Do NOT include
PASS_POST_BUMP_RIGGED (post-deferred overlay bump, different semantics).

## D. "No glow"
Fallback SHOULD run the overlay GLTF glow sweep (harvested, RIGGED_GLOW, overlay_mask=ALL). Likely
meanings: (1) flat metallic mistaken for glow -> fixed by fullbright-shiny restore; (2) no real PBR
emissive (check mGLTFMaterial->mEmissiveTexture/mEmissiveColor -- overlay skips empty-emissive batch);
(3) LEGACY PASS_GLOW_RIGGED not harvested = a REAL gap (vertex EMISSIVE attribute -> bloom alpha via
gDeferredEmissiveProgram; the GLTF RGB sweep is not a replacement) -> later RIGGED_GLOW producer;
(4) verify source group actually has PASS_GLTF_GLOW_RIGGED not just a bright base texture; (5) overlay
additive RGB vs stock bloom-alpha are different channels -- check both. Add per-clone glow diagnostics.

## E. Revised build order
1. **SOLID COMPLETION RESCUE (NOW, one slice):** cross-phase pending coverage + PASS_BUMP_RIGGED
   G-buffer + PASS_SHINY_RIGGED->simple normalization + shared staged forward dispatcher foundation +
   FULLBRIGHT + FULLBRIGHT_ALPHA_MASK + FULLBRIGHT_SHINY + finalize RIGGED_SOLID after forward stages.
   Fastest visible restore of real wardrobes, hole-free. (Bump alone is NOT independently useful --
   fullbright/fb-shiny keep blocking the bit.)
2. **Static opaque/masked deferred** (STATIC_SOLID) before blend (collars/masks into world depth).
3. **Generalize forward infra for alpha** (alpha shader prep, water-sign/stage, per-draw blend
   factors, clone sort, depth-writes off) -- a GENERALIZATION of the dispatcher, not a 2nd framework.
4. **Rigged blend** (PASS_ALPHA_RIGGED incl GLTF blend, RIGGED_BLEND all-success, gated on solid).
5. **Static blend** (reuse alpha dispatcher, non-rigged variants + object transforms).
6. **Glow completion** (GLTF glow coverage + legacy PASS_GLOW_RIGGED harvest + bloom-alpha semantics).
7. **Locked clone** last (temporal ownership + texture mutation, not this coverage failure).

No good cheap G-buffer approximation for fullbright shiny (writing fullbright into a lit G-buffer
changes semantics; PBR invents params; diffuse loses reflection). Per-batch coverage REJECTED as
interim (needs stable batch identities/subdraw success/overlay batch-filtering; substantial, still
leaves flat gold, creates a 2nd suppression model forward submission would replace).

Codex session: 019f8cc2-b6d3-7430-9d1e-3de61a51d5bb
