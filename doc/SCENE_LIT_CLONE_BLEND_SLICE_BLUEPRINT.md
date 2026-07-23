# Scene-Lit Clone — Blend Slice Blueprint (Codex, 2026-07-22 night)

Design consult result for the BLEND slice (scene-lit rigged hair/sheer), delivered after coverage
Slice 1 (`529baf45cdd`). **Codex re-ordered the plan: static-SOLID deferred lands FIRST.**

## ⭐ Revised slice order (Codex recommendation, adopted)
1. **Slice 2A — static opaque/masked deferred** (`STATIC_SOLID` coverage). Opaque attachments
   (collar/mask/jewelry/shells) must be in WORLD DEPTH before rigged blend composites, or a
   UI-fallback static drawn later overwrites already-composited scene-lit hair with no depth.
2. **Slice 2B — shared forward-alpha infrastructure.** Factor `prepare_alpha_shader()`
   (lldrawpoolalpha.cpp:88 file-local) into a reusable helper; both blend slices need it.
3. **Slice 2C — rigged blend submission** (`RIGGED_BLEND` coverage) — the meat, below.
4. **Slice 3 — static BLENDED faces** through the same forward plumbing (`STATIC_BLEND`).

## Slice 2C core design (rigged blend)
- **Entry point:** `LLPipeline::renderGhostDeferredRiggedBlend(const LLCamera&)` called INSIDE
  `renderDeferredLighting()` at pipeline.cpp:13243, right after
  `renderGeomPostDeferred(*LLViewerCamera::getInstance())`, BEFORE `popRenderTypeMask()` +
  `screen_target->flush()`. NOT in llviewerdisplay.cpp (target already flushed there). Wrap in its
  own `LLScopedGhostRenderInvariant` ("renderGhostDeferredRiggedBlend").
- **Pass reality check:** GLTF `ALPHA_MODE_BLEND` registers as `PASS_ALPHA` -> rigged `type+1` ->
  `PASS_ALPHA_RIGGED`. The collector's `PASS_MATERIAL_ALPHA_RIGGED`/`*_BLEND_RIGGED` entries are
  effectively dead-map compatibility; live blended content arrives as `PASS_ALPHA_RIGGED`.
  **Do NOT infer shaders from mPass — inspect the LLDrawInfo** (mirror LLDrawPoolAlpha::renderAlpha):

  | Batch condition | Shader (.mRiggedVariant) |
  |---|---|
  | mGLTFMaterial, ALPHA_MODE_BLEND | gDeferredPBRAlphaProgram |
  | legacy mMaterial | gDeferredMaterialProgram[mShaderMask] (blend perms 1/5/9/13; use STORED mask) |
  | no material, not fullbright | gDeferredAlphaProgram |
  | fullbright | gDeferredFullbrightAlphaMaskAlphaProgram |

- **Shader prep:** the factored prepare_alpha_shader equivalent (DISPLAY_GAMMA, waterSign,
  WATER_WATERPLANE, MINIMUM_ALPHA, mCanBindFast=false, base+rigged variants), then
  `gPipeline.bindDeferredShaderFast(*shader)` per shader switch (full deferred env/sun/shadow/probe
  bind — NOT just bindLightFunc).
- **Per-batch:** uploadMatrixPalette; TexSetup semantics minus applyModelMatrix; per-draw blend
  factors (RGB = mBlendFuncSrc/mBlendFuncDst, alpha = ZERO/ONE_MINUS_SRC_ALPHA); legacy material
  uniforms (SPECULAR_COLOR/ENVIRONMENT_INTENSITY/EMISSIVE_BRIGHTNESS); fullbright binds
  EXPOSURE_MAP; GLTF binds shader FIRST then mGLTFMaterial->bind(mTexture), respect mDoubleSided,
  scalar-only (alpha blend is never multi-material batched). MINIMUM_ALPHA = stock low discard
  floor, not a mask cutoff.
- **State:** `LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL)` — depth writes OFF (stock rigged
  alpha writes depth, but off is deliberate: first layer must not reject later layers; opaque
  deferred body already supplied occluder depth). No new GL_FRAMEBUFFER_SRGB scope (forward screen
  target pass; shaders advertise hasSrgb). Restore blend factors to CAPTURED incoming, not assumed
  BT_ALPHA.
- **Sorting:** LLDrawInfo has NO authoritative mDistance (that's groups/faces, source-camera-relative)
  and stock rigged alpha preserves draw order within an avatar. Rule: sort clone INSTANCES
  far-to-near by transformed proxy bounds (UUID tie-break); preserve in-clone draw-map order.
  Accepted artifact: fixed ordering vs world translucents (drawn after renderGeomPostDeferred,
  the clone blends over world alpha even when behind it); approximate interleave for intersecting
  clones. True global sorting = unified stream refactor of renderAlpha (out of scope).
- **Glow/emissive:** do NOT reproduce stock alpha's secondary bloom-alpha emissive accumulation in
  this slice — RIGGED_GLOW ownership stays with the overlay (double-glow risk otherwise).
- **Water:** classify above/below from CLONE transformed bounds (not source). Straddling clones =
  unsupported -> leave blend uncovered (overlay fallback). (Dual clipped submission = later option.)
- **Coverage:** any_blend_draw/all_blend_ok per instance, same all-eligible-success shape as
  Slice 1; failures include material/shader mismatch, non-blend GLTF material in category, texture
  binding validation, helper not drawing. Bit only on 0->1 transition; per-category counters.
- **Cross-phase dependency (conservative hole-free rule):**
  `RIGGED_SOLID covered -> attempt forward blend; RIGGED_SOLID not covered -> skip blend entirely`
  (fallback fullbright solid drawn later in UI phase would bury scene-lit hair; drawing blend but
  withholding the bit = double-alpha). Same gating vs present-but-uncovered STATIC_SOLID if rigged
  blend ever lands before static solid — hence the 2A-first order.

## Ranked risks
1. Cross-phase fallback conflict (the gating rule above is the mitigation)
2. World-translucent ordering (accepted artifact, document)
3. Shader-preparation drift (must factor + reuse prepare_alpha_shader, full bindDeferredShaderFast)
4. Blend-function fidelity (per-draw factors, not global BT_ALPHA)
5. Water classification from clone (not source) placement
6. Sorting approximation (instance-level far-to-near)
7. Emissive ownership (keep glow out of this slice)

## In-world validation checklist (Slice 2C)
Legacy alpha hair over covered body; all 4 legacy blend perms; GLTF ALPHA_MODE_BLEND (confirm
harvested as PASS_ALPHA_RIGGED); fullbright alpha (exposure map bound); double-sided GLTF hair;
custom blend factors; hair in front of AND behind own body; two clones at different depths, then
intersecting; clone behind/in front of opaque world; behind glass/particles (accepted artifact);
above/below water; palette-fail -> no bit + full overlay; incomplete shader/null material -> no bit;
solid-coverage-fail -> blend skipped + classic overlay complete; frozen/culled -> untouched;
contamination OFF/ON; toggle off -> byte-identical; counters (draws vs 0->1 instances); invariant 0;
glow channel unchanged before/after.
