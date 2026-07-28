# Ghost Clones — Overlay↔Entity Graphical Parity (Deep-Research Brief, with source)

**Audience:** an LLM doing deep research (e.g. ChatGPT). Everything needed is inline —
the two rendering paths, the actual current source of the cheap "overlay" path, the
material/alpha/PBR replication helpers, the coverage model, and the pipeline API —
followed by the research objective.

**The viewer:** Alchemy-Machinima, a fork of the Alchemy Second Life viewer (Linden
Lab / Firestorm lineage) tuned for machinima. Single-process C++/OpenGL, deferred
renderer with a GLTF-PBR path and legacy Blinn-Phong path, `LLAppViewer::idle()` +
`display()` once per frame. Avatars are skinned rigged meshes; a rigged mesh's matrix
palette bakes every vertex into WORLD space (`invBind * jointWorld`), which the overlay
exploits.

---

## 0. The objective

Ghost Studio can spawn a clone of an avatar in two ways:

- **Entity clone** (`LLGhostAvatar`): a *real* client-only `LLVOAvatar`. It renders
  through the FULL scene path — deferred lighting, shadows, GLTF-PBR, every alpha mode —
  so it is **graphically correct** (rigged mesh heads with alpha modes, rigged bodies,
  rigged PBR materials all mirror faithfully). It is **expensive**: a whole extra avatar
  (skinning, deferred, shadows, PBR, cloned attachments as real client-only objects).

- **Overlay ghost**: the cheap path. It **harvests the SOURCE avatar's already-built
  rigged draw batches** and re-draws them at the ghost's spot, mostly through a
  bespoke, **diffuse-only** shader (`gActorGhostProgram` / `actorghostF.glsl`), plus a
  partial submission of opaque/masked rigged geometry into the real deferred G-buffer.
  It is **cheap** (no second avatar, reuses the source's vertex buffers + world-baked
  palette) — the FPS win — but it does **not** achieve parity: rigged mesh heads with
  alpha modes, rigged bodies, and rigged PBR materials **mirror wrong** (wrong/black/
  white textures, broken masks, missing PBR shading).

**We already have Entity working. We need Overlay to reach graphical parity with Entity
— faithful rigged alpha-mode + rigged PBR mirroring — while staying materially cheaper
than a full avatar clone.** That is the whole point (save FPS while filming crowds).

**Sharpened goal:** the overlay CLONE must look **identical to the source avatar — exactly
what the Entity clone already achieves — i.e. full opaque photographic fidelity, NOT a
translucent or stylized "ghost."** The bespoke diffuse-only shading is therefore not a
requirement to preserve for the clone; it is the very thing to **replace**. (The stylized
FX looks — hologram / x-ray / wireframe — are a separate, orthogonal feature and are NOT a
constraint on clone parity.) So the expected answer is almost certainly "render the
harvested rigged batches through the REAL material/PBR/alpha shaders at the ghost
transform," not "perfect the hand-reconstructed diffuse approximation."

**Deliverable:** a prioritized, concretely-scoped plan to close the parity gap, with
specific approaches keyed to the code below and the constraints in §8. For each: the
change, where it hooks in, the cost impact (must stay cheaper than a full `LLGhostAvatar`),
risk, and effort. Say plainly which gaps are closable in the overlay model and which
fundamentally require the real avatar shaders — and if the answer is "drive the real
rigged material/PBR shaders at the ghost transform," design exactly that.

---

## 1. The two paths

### 1.1 Entity clone — the correct, expensive reference (`LLGhostAvatar`)

A real `LLVOAvatar` with `mIsDummy = false` (unlike `LLControlAvatar`/`LLUIAvatar`),
so it takes the full avatar pipeline. From `llghostavatar.h`:

> "A must NOT [set mIsDummy]: taking the real path is the entire point, because that is
> what earns deferred lighting, cast/received shadows, fog, tonemap and ReShade
> visibility."

It `cloneAppearanceFrom()` (shape + baked textures) and `cloneAttachmentsFrom()` (worn
mesh as real client-only prims). Because it is a genuine avatar, the stock deferred +
GLTF-PBR + alpha-mode shaders render it correctly with **zero** bespoke material code.
Cost = a whole avatar. This is the fidelity target.

### 1.2 Overlay ghost — the cheap path (harvest + re-draw)

Two cooperating halves:

**(a) Deferred G-buffer submission** (`LLPipeline`, `pipeline.cpp`): submits the clone's
opaque + alpha-masked rigged batches into the real deferred G-buffer so they get true
scene lighting/material (this is the "scene-lit clone" work). API (`pipeline.h`):

```cpp
void renderGhostDeferredOpaqueMasked(const LLCamera& camera);          // into G-buffer
enum class EGhostForwardStage : U8 { /* fullbright/shiny/mask/glow ... */ };
void renderGhostPostDeferred(const LLCamera& camera, EGhostForwardStage stage);
bool ghostPostDeferredSolidsPending(const LLCamera& camera) const;
GhostCoverageMask getGhostDeferredCoverageThisFrame(const LLUUID& instance_id) const;
```

**(b) Forward overlay** (`LLActorMover`, `llactormover.cpp`): re-draws the remaining
categories (blend, additive glow, static faces) with the bespoke `gActorGhostProgram`,
coloring ONLY what the deferred half did not cover.

The two are coordinated by a **5-category coverage mask** so a batch is never drawn
twice and never drawn nowhere (`llghostcoverage.h`):

```cpp
enum EGhostCoverage : U32 {
    GHOST_COVERAGE_NONE         = 0,
    GHOST_COVERAGE_RIGGED_SOLID = 1u<<0,  // rigged opaque + cutoff-masked
    GHOST_COVERAGE_RIGGED_BLEND = 1u<<1,  // rigged alpha-blended
    GHOST_COVERAGE_RIGGED_GLOW  = 1u<<2,  // rigged additive emissive
    GHOST_COVERAGE_STATIC_SOLID = 1u<<3,  // non-rigged attachment faces, opaque + masked
    GHOST_COVERAGE_STATIC_BLEND = 1u<<4,  // non-rigged attachment faces, alpha-blended
    GHOST_COVERAGE_ALL = /* all five */
};
bool ghost_pass_is_blend(U32 pass);
bool ghost_pass_is_glow(U32 pass);
```

> "The deferred submission records WHICH render categories it actually drew into the
> G-buffer; the overlay then colors ONLY the categories deferred did NOT cover. A single
> all-or-nothing 'submitted' bit is exactly wrong: one rigged opaque batch drawing must
> not suppress the clone's blended hair, additive glow, or static attachment faces —
> they would render NOWHERE (face-shaped holes / broken hair / vanishing attachment)."

**Net:** opaque/masked rigged (incl. opaque PBR) can reach the real deferred shaders
(good). **Blended, additive/emissive, PBR-blend, and static faces fall to the bespoke
diffuse-only overlay shader — that is where parity breaks.**

---

## 2. The overlay shader is diffuse-only (`actorghostF.glsl`, complete)

This one program covers every ghost style (clone / hologram / x-ray / wireframe). Note
it samples exactly ONE `diffuseMap`, applies a flat `color`, an alpha-mask cutoff
(`ghostAux.x`), a per-slot filter (`ghostSlot`), and FX. **No metallic-roughness, no
normal map, no emissive map, no occlusion, no PBR BRDF, no proper sRGB/tonemap** — so a
rigged PBR face can at best show its base-color texture, unlit and flat:

```glsl
out vec4 frag_color;
uniform vec4 color;
uniform sampler2D diffuseMap;
uniform float ghostTime;
uniform vec4 ghostParams;   // scanline/rim/flicker/period
uniform vec4 ghostAux;      // x=alpha-mask cutoff, y=texture-RGB mix (1=clone), z=pixelate, w=phase
uniform vec4 ghostFx;       // shimmer/glitch/brightness
uniform int  ghostSlot;     // >=0: draw only frags whose per-vertex material slot matches; -1 off
in vec2 vary_texcoord0; in vec3 vary_position; in vec3 vary_normal;
flat in int vary_texture_index; in vec4 vary_vertex_color;
void main() {
    if (ghostSlot >= 0 && vary_texture_index != ghostSlot) discard;   // indexed multi-material redraw
    vec2 uv = vary_texcoord0.xy;
    /* optional pixelate + glitch tears omitted */
    vec4 tex = texture(diffuseMap, uv);
    tex *= vary_vertex_color;                       // legacy TE tint/alpha; white for PBR
    if (tex.a < ghostAux.x) discard;               // authored alpha-mask cutoff
    /* scanline/rim/flicker FX omitted */
    vec3 base = mix(vec3(1.0), tex.rgb, clamp(ghostAux.y,0.0,1.0)) * color.rgb;  // clone = show tex.rgb
    vec3  rgb   = (base * scan * flicker + color.rgb * rim) * max(ghostFx.w, 0.0);
    float alpha = clamp(color.a * tex.a * (...) * flicker + rim*0.5, 0.0, 1.0);
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
```

Because the shader has none of the PBR material uniforms, the overlay must reconstruct
material state **by hand on the CPU** and feed it in as a single texture + tint + cutoff
+ a manually-composed UV matrix (next section). Any miss = wrong mirroring.

---

## 3. Hand-reconstructed material replication (`llactormover.cpp`, complete helpers)

This is the heart of the parity problem: the overlay re-derives, per harvested batch,
what the real render would have bound. It is a partial re-implementation of the
avatar/PBR material pipeline.

```cpp
// Multi-material INDEXED batches (e.g. a head merging eye materials): the vertex
// buffer's texture_index selects the material per vertex, so ONE bound texture is
// wrong for every non-anchor slot ("eye materials came out white"). The clone
// redraws the batch once per slot with the ghostSlot shader filter.
S32 ghost_batch_slot_count(LLDrawInfo* di) {
    if (di->mGLTFMaterialList.size() > 1)  return (S32)di->mGLTFMaterialList.size();
    if (di->mMaterialSlotList.size() > 1)  return (S32)di->mMaterialSlotList.size();
    if (di->mTextureList.size() > 1)       return (S32)di->mTextureList.size();
    return 1;
}

// Resolve one slot: colour texture (or emissive map), mask cutoff, colour factor
// (GLTF base/emissive colour, LINEAR — caller gamma-approximates). nullptr = gap slot.
LLViewerTexture* ghost_batch_slot_texture(LLDrawInfo* di, S32 slot, bool emissive,
                                          F32& out_cutoff, LLColor4& out_factor) {
    out_cutoff = 0.f; out_factor = LLColor4::white;
    if (di->mGLTFMaterialList.size() > 1) {
        LLFetchedGLTFMaterial* m = /* di->mGLTFMaterialList[slot] or nullptr */;
        if (!m) return nullptr;
        if (m->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK) out_cutoff = m->mAlphaCutoff;
        if (emissive) { out_factor = LLColor4(m->mEmissiveColor,1.f); return m->mEmissiveTexture.get(); }
        out_factor = m->mBaseColor; return m->mBaseColorTexture.get();
    }
    if (di->mMaterialSlotList.size() > 1) {
        const LLDrawInfo::MaterialSlot& s = di->mMaterialSlotList[slot];
        out_cutoff = s.mAlphaMaskCutoff; return s.mDiffuse.get();
    }
    if ((size_t)slot < di->mTextureList.size()) return di->mTextureList[slot].get();
    return nullptr;
}

// Scalar batch base texture the way the real render binds it: PBR base-colour map
// unless a media override (mTexture) is present; legacy diffuse otherwise.
LLViewerTexture* ghost_batch_texture(LLDrawInfo* di) {
    if (di->mGLTFMaterial.notNull()) {
        if (di->mTexture.notNull()) return di->mTexture.get();        // media override on PBR
        return di->mGLTFMaterial->mBaseColorTexture.get();
    }
    return di->mTexture.get();
}

// Alpha-mask cutoff for the shader discard: GLTF authored cutoff for PBR mask, the
// (normalized) LLDrawInfo cutoff for legacy mask, 0 otherwise.
F32 ghost_batch_cutoff(LLDrawInfo* di, U32 pass) {
    if (!ghost_pass_is_mask(pass)) return 0.f;
    if (di->mGLTFMaterial.notNull()) return di->mGLTFMaterial->mAlphaCutoff;
    return di->mAlphaMaskCutoff;
}

// Rebuild texture_matrix0: the legacy SL texture-anim matrix composed with the GLTF
// KHR_texture_transform of the base-colour map — because the PBR shaders apply that
// from uniforms the ghost shader does NOT have. This is the closed form of
// textureUtilV.glsl's texture_transform() (flip / offset*rot*scale / flip) collapsed
// into one affine in row-vector convention.
bool ghost_compose_khr_uv(const LLGLTFMaterial::TextureTransform& tt, LLMatrix4& out) {
    if (tt.mOffset==0 && tt.mScale==1 && tt.mRotation==0) return false;
    const F32 c=cosf(tt.mRotation), s=sinf(tt.mRotation), sx=tt.mScale.x, sy=tt.mScale.y;
    LLMatrix4 k;
    k.mMatrix[0][0]= c*sx; k.mMatrix[0][1]= s*sx;
    k.mMatrix[1][0]=-s*sy; k.mMatrix[1][1]= c*sy;
    k.mMatrix[3][0]= s*sy + tt.mOffset.x; k.mMatrix[3][1]= 1.f - c*sy - tt.mOffset.y;
    out *= k; return true;
}
bool ghost_batch_uv_matrix(LLDrawInfo* di, LLMatrix4& out) {
    bool have=false;
    if (di->mTextureMatrix) { out=*di->mTextureMatrix; have=true; } else out.setIdentity();
    if (di->mGLTFMaterial.notNull())
        have |= ghost_compose_khr_uv(di->mGLTFMaterial->mTextureTransform[GLTF_TEXTURE_INFO_BASE_COLOR], out);
    return have;
}
```

**The re-render** (`LLActorMover`, design comment verbatim — the scope is explicit):

> "TRUE 3D ghost: re-render the actor's own worn rigged geometry ... The seam that makes
> this cheap: a rigged mesh's matrix palette already bakes every vertex into WORLD space,
> so placing the ghost elsewhere is just a world-space translation premultiplied into the
> modelview — no per-vertex work, no second skeleton. ... CLONE — UNLIT textured copy:
> each batch re-binds its own resolved colour map (legacy diffuse or PBR base colour) and
> draws near-white * base-colour-factor so the texture reads as authored. Opaque + masked
> batches draw unblended (mask holes come from the shader discard); the batches the real
> render alpha-BLENDS get their own blended sweep over the primed body. **This is a
> fullbright-style clone; a scene-LIT clone needs deferred-pass integration (gbuffer +
> light apply) and is future work.** ... SCOPE: covers WORN MESH (rigged attachments)…"

---

## 4. Why parity breaks (precise failure model)

| Face class | Entity clone | Overlay ghost | Result |
|---|---|---|---|
| Rigged opaque (legacy) | real deferred | deferred G-buffer submission | ~parity |
| Rigged opaque **PBR** | real GLTF-PBR deferred | deferred submission (`pushGhostGLTFBatchIndexed`) | close, verify sRGB/factor |
| Rigged **alpha-mask** head | real masked shader | shader `discard` at hand-resolved cutoff | breaks if cutoff/slot/UV mis-resolved |
| Rigged **alpha-blend** (hair, skirts) | real blended shader | bespoke diffuse-only blended sweep | **no PBR, unlit → wrong** |
| Rigged **PBR blend / emissive** | real PBR/emissive | base-colour or emissive-only, flat | **wrong** |
| Multi-material indexed (heads) | real per-slot | per-slot redraw via `ghostSlot` | fragile; gaps → holes/white |
| Static (non-rigged) attachment faces | real | overlay static sweep | approximate |

Root causes:
1. **The overlay shader is diffuse/base-colour-only and unlit** — it cannot represent the
   GLTF-PBR BRDF (metallic/roughness/normal/emissive/occlusion), correct sRGB decode, or
   scene lighting. Blended/emissive/PBR-blend rigged faces route here.
2. **Material state is re-implemented on the CPU** (`ghost_batch_slot_texture`,
   `ghost_batch_cutoff`, `ghost_batch_uv_matrix`, KHR-transform closed form, base-colour
   factor, indexed slot iteration). Every param the real shader would apply must be
   re-derived by hand; misses = wrong texture/UV/mask ("mirroring wrong").
3. **Alpha modes are hand-classified** into mask (shader discard) vs blend (separate
   sweep) vs glow (additive), coordinated with the deferred coverage mask across all
   `PASS_*_RIGGED` variants. Rigged heads (multi-material, masked, often PBR) hit the
   worst-case of all three at once.

The Entity clone has none of these problems because it never re-implements anything — it
IS an avatar, so the stock shaders do it.

---

## 5. The pipeline coverage/lighting machinery (for context)

`pipeline.h` maintains, per frame, a per-instance coverage map plus a "submission
progress" ledger so a category is finalized only when every eligible pass drew, and no
observer sees premature coverage:

```cpp
std::map<LLUUID, GhostCoverageMask> mGhostDeferredCoverage;   // what the G-buffer covered
struct GhostSubmissionProgress { GhostCategoryProgress mRiggedSolid; /* blend reserved */ ... };
std::map<LLUUID, GhostSubmissionProgress> mGhostSubmissionProgress;
void finalizeGhostRiggedSolidCoverage();
```

There is also a live render-invariant guard and a one-shot contamination/diagnostics
test (`llghostdeferreddiagnostics.*`, `LLScopedGhostRenderInvariant`) proving ghost
draws don't leak into the real scene. Settings gate the whole thing (`GhostDeferredEnable`).

---

## 6. What Entity does that Overlay must match

Entity renders each rigged batch through the **stock rigged material/PBR shaders** with
the batch's real material bound by the engine's own `LLRenderPass::pushBatch` /
`LLFetchedGLTFMaterial::bind` path — so base-colour texture + factor + sRGB, KHR UV
transform, metallic/roughness/normal/emissive/occlusion maps, alpha mode + cutoff,
double-sided, and deferred scene lighting are all applied by the same code the whole
world uses. Overlay parity means reproducing that binding + shading for the harvested
batches at the ghost's transform/palette, instead of the diffuse-only approximation.

---

## 7. Research questions (the deliverable)

**A. The central architectural choice.**
1. Can the overlay drive the **real rigged material/PBR shaders** (the same programs the
   avatar deferred + alpha passes use) over the *harvested* batches at the ghost's
   world-baked palette + translated modelview — instead of the bespoke diffuse-only
   `gActorGhostProgram`? What exactly must be rebound (skin palette, material, UV, alpha
   mode, deferred vs forward stage) and what stops this today?
2. Alternatively, **widen the deferred G-buffer submission** (`renderGhostDeferredOpaqueMasked`
   / `renderGhostPostDeferred`) to cover ALL rigged categories — blend, PBR-blend,
   emissive/glow, plus static — so the coverage mask reaches `GHOST_COVERAGE_ALL` and the
   diffuse-only overlay is used only for the intentionally-stylized FX modes. What are the
   ordering/blending hazards (deferred can't hold alpha-blend; forward blend after light
   apply; emissive/glow additivity) and the correct stage sequence?
3. Is there a hybrid: keep the overlay for stylized ghosts, but for the **CLONE style**
   route every category through real shaders? Where's the clean seam?

**B. Getting the material replication exactly right (if the bespoke path must remain).**
4. PBR base-colour: correct sRGB decode + `baseColorFactor` + KHR UV so a rigged PBR head
   mirrors 1:1 with the deferred path. What's missing vs `LLFetchedGLTFMaterial::bind`?
5. Alpha modes: verify mask cutoff, blend sweep, and double-sided across every
   `PASS_*_RIGGED` variant (the lists at `pipeline.cpp:3601-3607, 4609-4632, 5173-5197`);
   which variant/material combos still mis-resolve on real heads/bodies?
6. Multi-material indexed batches: is per-slot `ghostSlot` redraw complete and
   gap-safe, or should indexed batches be split/handled like the real indexed PBR draw?

**C. Cost — parity must stay cheaper than Entity.**
7. Quantify the cost delta: overlay (reuse source VBs + world-baked palette, N extra
   draws) vs a full `LLGhostAvatar` (skinning + deferred + shadows + PBR + cloned
   attachments). If parity means "real shaders over harvested batches," how much of the
   FPS advantage survives, and which avatar costs are avoided (no second skeleton, no
   shadow passes, no attachment object churn, shared textures)?
8. Where does the deferred-submission approach scale better/worse than N overlay draws
   for a crowd of clones?

**D. Correctness / integration.**
9. Preserve the coverage invariant + contamination guarantee (`llghostdeferreddiagnostics`)
   under any widening — no double-draw, no render-nowhere holes, no leak into the real scene.
10. Interaction with the new **World Time Scale** (LLPresentationTime): ghost animation is
    already client-driven; ensure parity work doesn't regress that.
11. Verification: how to prove overlay == entity per face class (a diff harness: spawn one
    Entity and one Overlay from the same source, same pose/camera, compare).

---

## 8. Constraints any proposal must respect

- **Engine:** C++/OpenGL, deferred renderer (G-buffer + light apply) with GLTF-PBR and
  legacy Blinn-Phong paths; rigged batches are `LLDrawInfo` with a world-baked matrix
  palette; passes are `LLRenderPass::PASS_*` (rigged variants enumerated in `pipeline.cpp`).
  Real material binding is `LLRenderPass::pushBatch` + `LLFetchedGLTFMaterial::bind`.
- **The overlay's cheapness comes from reusing the source avatar's existing VBs + palette
  and NOT instantiating a second avatar.** Any parity fix must preserve a real cost
  advantage over `LLGhostAvatar`, or it defeats the purpose.
- **Coverage model is load-bearing** (`llghostcoverage.h`): the deferred half and the
  overlay half must never double-draw or leave a category rendering nowhere; keep the
  live invariant + diagnostics green.
- **The CLONE style must look identical to the source (= Entity), full opaque fidelity** —
  it need NOT preserve the diffuse-only path; replacing that path for the clone is the
  expected outcome. The stylized FX modes (hologram / x-ray / wireframe) are a separate,
  orthogonal feature, not a constraint on clone parity.
- Client-side only (no simulator changes); single-threaded render dispatch; gated behind
  `GhostDeferredEnable`.

*(Source above is current, from `actorghostF.glsl`, `llactormover.cpp` (ghost batch
helpers + the true-3D re-render), `llghostcoverage.h`, `llghostavatar.h`, and the
`LLPipeline` ghost API in `pipeline.h`. The full bodies of
`renderGhostDeferredOpaqueMasked` (`pipeline.cpp:5142`) and `renderGhostPostDeferred`
(`pipeline.cpp:5566`) and the re-render (`llactormover.cpp:~3777+`) are large; cite them
by name for expansion.)*
