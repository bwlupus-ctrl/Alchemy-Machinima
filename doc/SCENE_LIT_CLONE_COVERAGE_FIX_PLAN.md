# Scene-Lit Clone — Coverage Fix Plan (handoff 2026-07-22)

Continuation point after the opaque widening landed (`c4f709830a1`). In-world test: rigged body +
head + PBR clothing now render scene-lit. Remaining issues DIAGNOSED by Codex (`bjq8ekps1`) —
**most are NOT bugs in the material/PBR draw code** (it matches stock). User directive: do the
**FULL, thorough fix**, defer to Codex.

## Codex diagnosis of the in-world symptoms
| Symptom | Root cause | Class |
|---|---|---|
| A. Legacy materials "look masked" (face-shaped holes) | Blended faces are harvested but the deferred pass SKIPS them (opaque/masked only) AND the overlay is fully suppressed once any rigged batch draws → blend faces render NOWHERE → holes that only *look* like masking. No MINIMUM_ALPHA leak; opaque/mask shader selection is correct. | Excluded-pass + all-or-nothing suppression |
| B. PBR texture misalignment | pushGhostGLTFBatch/Indexed match stock (transforms/UV/slot layout intact). Likely live source material/texture mutation or residency. Isolate via scalar-vs-indexed A/B. | Live-harvest limit |
| C. BOM layering wrong on rigged faces | `di->mTexture` IS the correct resolved texture; the clone just has no frozen snapshot of the bake (bake/rebake mutates it live). | Live-harvest limit → locked clone |
| D. Unrigged scripted (hide/show) attachment vanishes | Static faces (GhostStaticFace) ride ONLY the overlay; one rigged deferred batch → whole overlay (incl. static faces) suppressed. `renderStudioGhosts` `continue` at ~llactormover.cpp:5445. | Needs static-face deferred |
| E. Rigged hair broken | Hair is mostly BLENDED (PASS_ALPHA_RIGGED / *_BLEND_RIGGED / PBR ALPHA_MODE_BLEND) — excluded, then overlay suppressed → gone. | Excluded-pass |

**Core issue:** the per-instance "submitted" bit is too coarse. "One rigged batch drew" is treated as
"the whole instance drew," so blended + static faces get neither deferred nor overlay rendering.

## THE FULL FIX (what the user chose) — do via Codex loop, slice by slice
1. **Coverage-aware suppression (foundation). ✅ DONE 2026-07-22 (Codex-designed, 0 must-fix).**
   Five categories (`RIGGED_SOLID/BLEND/GLOW`, `STATIC_SOLID/BLEND`) in `llghostcoverage.h`; frame-
   stamped `map<LLUUID,GhostCoverageMask>` in LLPipeline; ALL-ELIGIBLE-SUCCESS rule (bit only when
   every overlay-solid batch deferred-drew — fullbright/shiny/bump block it, partial failure = full
   fullbright fallback, never holes); prime always covers present solids; overlay colors only
   `present & ~covered`; deferred queue now clone-style-only; per-category debug counters.
   `ghost_pass_is_blend/glow` exported (one classification domain). AWAITING in-world test.
2. **Blend slice (P5) — the proper scene-lit fix for hair/sheer clothing.** A forward-alpha deferred
   submission for BLENDED rigged faces (PASS_ALPHA_RIGGED, *_MATERIAL_ALPHA_RIGGED, *_BLEND_RIGGED, PBR
   ALPHA_MODE_BLEND), drawn AFTER renderDeferredLighting (forward-lit + depth-sorted), so blended layers
   render scene-lit and composite correctly. This is the bigger piece (needs sorting + the forward
   alpha shaders). Get the concrete shader/pass list + insertion point from Codex.
3. **Static-face deferred submission.** Submit opaque/masked GhostStaticFace entries into the deferred
   path under their attachment/object matrices (non-rigged variants of the same deferred programs), and
   re-collect the scripted hide/show TE/alpha state. Blended static faces go through the blend slice.
   Fixes symptom D. (Codex A + this diagnosis both call for it.)

After 1-3, blend/hair/static all render (scene-lit), no holes.

## Follow-ons (separate projects, already designed)
- **Locked/persistent clone** (fixes B + C properly + independence): OWNED snapshot — deep-copy VB +
  material VALUES + palette + COPY mutable BOM textures. Design in `doc/` (Codex B). L proof / XL full.
  **RE-CONFIRMED in-world 2026-07-22 (screenshots, 21:37 exe): PBR UV misalign on helm/mask surfaces,
  head bake smooth/featureless (live BOM not frozen), head-worn object textures off — all symptom B/C.**
- **Non-self source-residency pin** (impostored/off-camera non-self): transient per-source-UUID pin
  gating isImpostor/shouldImpostor/computeUpdatePeriod/LOD. Design in Codex A consult.
- **System-avatar body** (classic bodies / mesh-head-only): the LLDrawPoolAvatar renderSkinned path.

## State at handoff
- Branch `develop`. All committed, tree clean. Scene-lit commits: P1 `0a91fa42489`, P0.4 contamination
  `5f1b472a45e`, opaque widening `c4f709830a1`. Exe built 21:37 has all of it.
- Proof layer works: live `LLScopedGhostRenderInvariant` (invariantViol=0 in-world) + one-shot
  `GhostDeferredContaminationTest` (INCONCLUSIVE on a LIVE clone because it animates -- that's the test
  refusing to bluff; would need a union-mask relax to PASS on a live clone, OR a still/locked source).
- Toggles: `GhostDeferredEnable` (on), `GhostDeferredDebugLog` (counter + invariant verdict).
- MANDATORY: all viewer code through Codex (see CLAUDE.md); user directive "works right > feature",
  defer to Codex multiple times, parallel consults OK.
