# Research brief — GhostLook: per-clone stylized render modes in a Second Life viewer

**Audience:** ChatGPT deep research
**Project:** `bwlupus-ctrl/Alchemy-Machinima` (fork of Alchemy Viewer / Linden Lab Second Life
viewer), branch `develop`. C++ / **OpenGL** (GLSL), deferred renderer.
**Goal:** give individual client-only avatar clones distinct **visual styles** — Hologram, Chrome,
Toon, Silhouette, Apparition, Dissolve — rendered per-clone, without disturbing how the rest of the
scene (or the real avatars) render.

---

## 1. Context: what a "clone" is here

"Ghost Studio" spawns **client-only avatar clones** for machinima. `LLGhostAvatar`
(`indra/newview/llghostavatar.{h,cpp}`) derives from `LLVOAvatar` but is synthetic: the simulator
knows nothing about it, it receives no ObjectUpdates, it is unparented, and it is placed by fiat.
`ALGhostStudio` (`indra/newview/alghoststudio.{h,cpp}`) holds per-instance state.

Crucially, **entity clones render through the REAL avatar path** — the deferred G-buffer, with full
scene lighting and shadows (this was hard-won work: see `doc/SCENE_LIT_CLONE_*.md`). They are not
overlay/billboard effects. Rigged mesh bodies, fitted-mesh attachments, PBR/GLTF materials and
baked (BOM) textures all render as they would for a real avatar.

There is also an older, separate **overlay ghost** type with simple stylization (hue/alpha/shimmer/
pixelate/glitch) drawn outside the deferred path. Those overlay-only controls do **not** apply to
entity clones, and the UI now hides them for entity selections. **This brief is about entity clones.**

## 2. What already exists (scaffold only)

The enum and per-instance plumbing are in place; **the shader work was deliberately deferred**:

- `ALGhostStudio::EGhostLook` (`alghoststudio.h:77`):
  `LOOK_NORMAL, LOOK_APPARITION, LOOK_HOLOGRAM, LOOK_CHROME, LOOK_TOON, LOOK_SILHOUETTE`
- per-instance `mLook` (`alghoststudio.h:221`) and `setInstanceLook()` (`:292`)
- clone-side `LLGhostAvatar::setEntityLook(S32 look, F32 alpha)` (`llghostavatar.h:115`) with
  `mEntityLook` / `mEntityLookAlpha` (`:167-168`)
- a UI selector already exists (currently labelled with `*` for the unimplemented entries)

So: the state, the UI and the per-clone routing exist. **What is missing is the actual rendering.**

## 3. The core research question

**How do you apply a per-object stylized shading override to ONE avatar inside a deferred renderer,
when that avatar is drawn by shared draw pools using shared shaders?**

The clone's geometry is submitted through the same `LLDrawPoolAvatar` / rigged batches / GLTF-PBR
material paths as every other avatar. There is no per-object shader slot. Naive approaches each have
problems:

- **Swap the shader per draw** — how, given batches are keyed by material/pool and the avatar pool
  binds its own programs? What is the cost of breaking batching for one avatar?
- **Override material values only** (e.g. force metallic=1, roughness=0 for Chrome) — cheap and may
  cover Chrome/Clay, but cannot express Toon, Silhouette, Hologram scanlines or Dissolve.
- **Post-process with a stencil/ID mask** — render the clone normally, tag its pixels, then run a
  full-screen stylization pass over just those pixels. Handles screen-space looks (scanlines, edge
  detection, dissolve) but is it compatible with SL's deferred + HDR + tonemap chain, and where in
  `renderFinalize` would it go?
- **A dedicated forward pass for styled clones** — full control, but loses scene lighting integration
  that was expensive to win, and must handle rigged skinning itself.

Please evaluate these (and any better option) **against SL's actual pipeline**, and recommend an
architecture — ideally one mechanism that covers most of the looks, with per-look shader variants
rather than six bespoke subsystems.

### Specific sub-questions
1. **Per-object identity in the G-buffer.** Is there a spare channel, stencil bit, or attachment
   slot in SL's `deferredScreen` (`0=albedo, 1=ORM, 2=normals, 3=emissive`) usable to tag "this pixel
   belongs to styled clone N"? If not, what is the cheapest way to obtain a per-clone mask —
   stencil, a separate ID render target, or re-rendering the clone to an offscreen buffer?
2. **Shader variant management.** How does this codebase (`LLViewerShaderMgr`, `make_rigged_variant`,
   the class1/class2/class3 shader levels) create and select program variants? What is the least
   invasive way to add styled variants for the avatar/rigged/PBR programs, and how many permutations
   does that actually imply?
3. **Which looks belong in which stage.** Classify each target look as (a) material-value override,
   (b) modified surface shading in the G-buffer pass, or (c) screen-space post effect:
   Hologram (additive scanlines, fresnel rim, flicker, chromatic offset) · Chrome (metallic=1,
   roughness≈0, env-reflective) · Toon (cel bands + Sobel/normal-depth outline) · Silhouette (flat
   fill + optional backlit rim) · Apparition (translucent + fresnel rim + inner glow) ·
   Dissolve (noise-threshold with emissive burn edge).
4. **Transparency.** Apparition/Dissolve need translucency, but a deferred G-buffer is opaque by
   nature and SL's alpha path is separate. What is the correct route for a *partially transparent
   but scene-lit* avatar here? (Note prior work found blended and static faces were the hard cases —
   see `doc/SCENE_LIT_CLONE_COVERAGE_FIX_PLAN.md`.)
5. **Shadows, reflections, impostors.** Should a styled clone still cast normal shadows and appear in
   reflection probes? What happens when it is impostored/LOD'd?
6. **Cost.** With 10–50 styled clones, what does each approach cost? Does per-clone stylization
   force per-clone draw calls or full-screen passes that do not amortize?
7. **Existing precedent in the tree.** `LLGhostAvatar` already applies a client-only **outer render
   transform** (foot-pivoted uniform scale) via `LLClientOuterTransform`, composed in
   `LLRenderPass::applyModelMatrix` and a scoped model-view override in `LLVOAvatar::renderSkinned()`.
   That proves per-clone render-state divergence is achievable. Is that same seam the right place to
   hang a style override, and what are its limits?

## 4. Constraints (hard)
- **Client-only.** Nothing may be sent to the simulator; no real object or avatar state may be
  mutated. Styling is local render only.
- **Must not disturb other rendering.** Real avatars, other clones set to `LOOK_NORMAL`, and the rest
  of the scene must render byte-identically. Prior sessions had bugs where clone-specific render code
  leaked cost or state into ordinary avatar paths — avoid that by construction.
- **Keep the scene-lit property where possible.** Losing real lighting/shadow integration would
  undo significant prior work; if a look genuinely requires abandoning it (e.g. Silhouette), say so
  explicitly.
- OpenGL, GLSL, and this fork's shader-level system (class1/2/3) — not D3D, not a new engine.

## 5. Deliverable
An architecture document with:
1. a recommended mechanism (with rejected alternatives and why);
2. per-look classification and, for 2–3 representative looks, concrete GLSL sketches;
3. a phased plan — smallest useful first (likely one look, one clone) then breadth;
4. cost analysis at 1 / 10 / 50 styled clones;
5. risks, and an explicit list of what could not be determined without reading more of the engine.

## 6. Sources worth consulting
- Linden Lab viewer source (the upstream this forks from):
  `github.com/secondlife/viewer` — `indra/newview/pipeline.cpp`, `lldrawpoolavatar.cpp`,
  `llviewershadermgr.cpp`, and `indra/newview/app_settings/shaders/` (class1/2/3 deferred shaders).
- Alchemy Viewer: `github.com/AlchemyViewer/Alchemy` (this fork's immediate parent, includes its own
  HDR/tonemap/post chain in `renderFinalize`).
- Firestorm: `github.com/FirestormViewer/phoenix-firestorm` for comparison of viewer-side render hacks.
- Second Life materials/PBR documentation (GLTF material support) for how PBR faces differ from
  legacy Blinn-Phong in the G-buffer.
- General technique references: stencil-masked post effects, ID-buffer/object-mask approaches,
  cel shading and normal-depth edge detection, octahedral/fresnel rim techniques, and dissolve
  shaders — mapped onto a **deferred** pipeline rather than forward.
- In-repo prior art (describe-only, the researcher will not have these files):
  `doc/SCENE_LIT_CLONE_RESEARCH_FINDINGS.md` (how clones were made to render through the deferred
  path at all), `doc/SCENE_LIT_CLONE_COVERAGE_FIX_PLAN.md` (which face categories were hard),
  `doc/AVATAR_UNIFORM_SCALE_DEEP_RESEARCH.md` (the outer-render-transform precedent).
