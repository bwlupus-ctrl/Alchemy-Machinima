# BDMerge — Forced / Alpha Masking Consolidation Brief

**Status:** read-only investigation. No source changed. This brief is the only artifact.
**Date:** 2026-07-11
**Scope:** forced client-side alpha masking (feature G2.3 / A2.3) and every stock alpha-mask
layer it stacks on, in `I:\alchemy-machinima`, cross-checked against Black Dragon
(`I:\black-dragon`).

---

## TL;DR verdict

- There are **not** "too many overlapping *forced-masking* implementations." There is exactly
  **one** forced-masking implementation (our G2.3), added at **3 code sites**, riding on top of
  the **stock Linden alpha-mask machinery**. What looks like layering is the normal
  stock mask pipeline (heuristic router → pass registration → per-batch cutoff →
  shadow reuse), which is **necessary infrastructure**, not redundant duplication.
- **A real BD forced-masking implementation does NOT exist.** BD's `canRenderAsMask()` is
  **byte-for-byte stock Linden**, BD has **no `BDMergeForceAlphaMask`-style setting**, and BD's
  cutoff-assignment sites are the plain stock `0.33`/`0.5` constants. The patch-log claim
  ("BD donor code no longer exists — dismantled in the PBR refactor") is **CONFIRMED**.
- "Strip Alchemy's approach and keep only Black Dragon's" is therefore **impossible as literally
  stated** — BD has nothing to keep. "Keep BD's" reduces to "revert G2.3 and run stock." The
  useful interpretation is: **the current G2.3 is already the single-predicate design the user
  wants**; the only genuinely justified consolidation is a **small cleanup** (collapse the two
  duplicated `LLCachedControl` reads and consider one internal predicate), not a rip-out.

---

## 1. Layer-by-layer map (our fork)

Masking decisions are made in **two phases**: (A) *routing* — which render pass a face lands in,
decided per-face at batch-build; (B) *cutoff* — the alpha threshold that pass uses, decided
per-batch. Then the shadow pass **reuses** the routing for free. Forced masking (G2.3) injects
into A and B; it does not add a third phase.

### Phase A — routing (which pass)

**A0. `LLDrawPool::POOL_ALPHA` gate.** Before masking is even considered, a face must be in the
alpha pool. `is_alpha` is set at `llvovolume.cpp:5568` (`getPoolType()==POOL_ALPHA ||
color.a < 0.999`) and the PBR override at `llvovolume.cpp:6155` routes non-BLEND PBR faces to
`POOL_GLTF_PBR` *before* alpha handling. Everything below only fires for faces that reached the
alpha pool.

**A1. Stock `LLFace::canRenderAsMask()` — the sole router.**
`indra/newview/llface.cpp:1175-1236`. This is the one predicate that decides "treat this
transparent face as a 1-bit mask instead of blending." Its stock heuristic ladder:
- `:1183` PBR early-out — `te->getGLTFRenderMaterial()` present → **return false** (PBR never
  masks here; it is routed by `mAlphaMode` elsewhere — see A4).
- `:1188` `LLPipeline::sNoAlpha` → **return true** unconditionally (the "render everything opaque"
  debug/precedent path; proves unconditional-true is a supported contract).
- `:1209` rigged faces → **return false** (stock never auto-masks rigged).
- `:1215` Blinn-Phong material with `DIFFUSE_ALPHA_MODE_BLEND` → **return false**.
- `:1221` the real heuristic: opaque face color (`a==1`) **and** no glow **and**
  `getTexture()->getIsAlphaMask()` (texture's alpha histogram qualifies) → return the
  `sAutoMaskAlphaDeferred` / `sAutoMaskAlphaNonDeferred` pipeline flag.

**A2. Our G2.3 forced-mask branch — inside the same function.**
`indra/newview/llface.cpp:1193-1207`. Inserted *after* the PBR and `sNoAlpha` early-outs, *before*
the rigged/BLEND/heuristic ladder. When `BDMergeForceAlphaMask` is on it returns `true` for any
face that is `(force_mask_rigged || !RIGGED)` **and** `color.a == 1.0` **and** `glow == 0`,
overriding creator BLEND and the texture heuristic. This is the **only** behavioral addition to
routing. It correctly sits behind the PBR early-out (so it cannot touch GLTF) and preserves the
"real transparency / glow keeps blending" carve-out.

**A3. Pass registration (consumers of A1/A2).** Three call sites read `canRenderAsMask()`:
- `llvovolume.cpp:6197` — genDrawInfo bucketing: mask-capable faces go to `sSimpleFaces`
  (the solid/mask batch list) instead of `sAlphaFaces`.
- `llvovolume.cpp:7260-7269` — `registerFace` into `PASS_FULLBRIGHT_ALPHA_MASK` (fullbright or
  `sNoAlpha`) vs `PASS_ALPHA_MASK`.
- `llvovolume.cpp:4605` — spatial-bounds `alpha_wrap` shrink decision (masked faces don't force
  the alpha shrink-wrap). This is a culling/bounds optimization, not a render decision.

There is also a **separate, independent** stock mask path for **non-alpha** faces whose Blinn-Phong
material is `DIFFUSE_ALPHA_MODE_MASK` (`llvovolume.cpp:7329-7331` and `:7352-7354`). This never
calls `canRenderAsMask()` — it is material-declared masking and is untouched by G2.3.

### Phase B — cutoff (what threshold)

**B1. Stock per-batch cutoff assignment (`genDrawInfo`).** `llvovolume.cpp`:
- material path `:5778` — `mAlphaMaskCutoff = mat->getAlphaMaskCutoff()/255`.
- non-material path `:5796-5803` — `0.5` for `PASS_GRASS`, else `0.33`.

**B2. Our G2.3 cutoff override — two sites, same function.**
- `llvovolume.cpp:5785-5792` — material branch: a forced BLEND face has a meaningless material
  cutoff (often 0), so when forced masking is on and it landed in a mask pass, use the global
  `BDMergeForceAlphaMaskCutoff` (default 0.5).
- `llvovolume.cpp:5807-5813` — non-material branch: while forced masking is on, the whole masked
  set follows the global slider instead of the 0.33/0.5 constants.

**B3. Cutoff consumption at draw (stock, unchanged).** The per-batch `mAlphaMaskCutoff` is pushed
to the shader in `lldrawpool.cpp`: `pushMaskBatches:529`, `pushRiggedMaskBatches:555`, and the
indexed/GLTF variant `pushMaskBatchesIndexed:606` (`mat_minimum_alpha` uniform array). G2.3 does
**not** touch these — it only changes the *value* stored in `mAlphaMaskCutoff` upstream at B2, so
rigged (`pushRiggedMaskBatches`) and indexed batches inherit the forced cutoff automatically.

### Phase C — passes & shadow reuse (stock, unchanged)

**C1. Color passes.** `PASS_ALPHA_MASK` / `PASS_FULLBRIGHT_ALPHA_MASK` are drawn via
`LLPipeline::renderMaskedObjects` / `renderFullbrightMaskedObjects`
(`pipeline.cpp:7848-7877` → `pushMaskBatches`), invoked at `pipeline.cpp:12233,12248`.

**C2. Shadow passes.** The shadow render-type set includes `RENDER_TYPE_PASS_ALPHA_MASK` /
`..._FULLBRIGHT_ALPHA_MASK` (and the `_RIGGED` variants) at `pipeline.cpp:11452-11456` and
`12669-12694`. Because shadows iterate the **same render maps** populated by the A3 registration,
a face forced into a mask pass is masked **identically in the shadow map** with the **same
`mAlphaMaskCutoff`**. This is the coherence guarantee that makes G2.3's single-router design work:
change one predicate, and color + shadow stay consistent for free. Nothing extra is needed and
nothing may be added here.

### How they stack (the "layers")

```
face in POOL_ALPHA? ──no──► (PBR non-blend → POOL_GLTF_PBR; else simple/shiny) — masking N/A
      │yes
      ▼
canRenderAsMask()  ┌─ PBR? → false            (A1, :1183)   PBR routed by mAlphaMode elsewhere
  (llface.cpp)     ├─ sNoAlpha → true          (A1, :1188)
                   ├─ [G2.3] forced → true     (A2, :1201)   ◄── our ONLY routing addition
                   ├─ rigged → false           (A1, :1209)
                   ├─ BLEND material → false    (A1, :1215)
                   └─ opaque+noglow+alphaTex?   (A1, :1221)  stock heuristic
      │true                              │false
      ▼                                  ▼
PASS_ALPHA_MASK / FULLBRIGHT       PASS_ALPHA (real blend)
  batch cutoff = 0.33/0.5/mat      (sorted, no depth write)
  └─ [G2.3] override → global slider (B2, :5785 / :5807)  ◄── our ONLY cutoff addition
      ▼
color pass (renderMaskedObjects) + shadow pass (same render map)  — automatic, coherent
```

So: **one stock router (A1)** + **one forced override in it (A2)** + **stock cutoff (B1)** +
**one forced override of it (B2, split in two spots)** + **stock passes/shadow reuse (C)**. The
"many layers" the user perceives are the stock pipeline stages, all of which are load-bearing.

---

## 2. What Black Dragon actually implements now

Verified directly against `I:\black-dragon` source:

- **`LLFace::canRenderAsMask()` — `black-dragon/indra/newview/llface.cpp:1186-1231`.**
  Byte-for-byte the stock Linden function: PBR early-out (`:1194`), `sNoAlpha` (`:1199`),
  rigged→false (`:1204`), BLEND→false (`:1210`), texture heuristic (`:1216`). **No forced
  branch, no `BDMerge`/`Force` setting read, no divergence from stock whatsoever.** (Diff it
  against our `llface.cpp:1175`: the *only* difference is our inserted `:1193-1207` block.)
- **Cutoff assignment — `black-dragon/indra/newview/llvovolume.cpp:5628,5632`.** Plain stock
  `0.5` (grass) / `0.33` (default). No global-cutoff override, no forced logic.
- **Settings — `black-dragon/.../app_settings/settings.xml`.** Grep for
  `ForceAlpha|ForcedAlpha|AlphaMaskForce|ForceMask` → **no matches**. BD ships no forced-masking
  toggle or UI.
- **Whole-tree grep** `ForceAlphaMask|ForcedMask|forceMask|force_mask` across
  `I:\black-dragon` → **zero files**.

**Conclusion:** BD's alpha-masking is **functionally identical to stock Linden**. There is **no
distinct, portable "BD forced-masking implementation."** The G2.3 patch-log claim is confirmed:
whatever BD once had (Niran's older forced-mask experiments) was dismantled in BD's PBR/GLTF
refactor, exactly as noted for A1.1 (shadow blur) and A1.3. Our G2.3 was written **novel to spec**,
not ported — and that was the correct and only possible call.

---

## 3. Redundancy diagnosis — is the user right?

**Partly, but not where they think.** There is no duplicate *forced-masking* engine to strip.
What exists:

| Layer | Redundant? | Verdict |
|---|---|---|
| Stock `canRenderAsMask()` heuristic (A1) | **No** — necessary router; also serves non-forced content | keep |
| G2.3 forced branch in `canRenderAsMask()` (A2) | **No** — the single feature predicate | keep |
| PBR `mAlphaMode` routing (A4 / `:6155`, `:1183` early-out) | **No** — genuinely separate pipeline; must stay separate | keep, do not merge |
| Non-alpha `DIFFUSE_ALPHA_MODE_MASK` path (`:7329`, `:7352`) | **No** — material-declared masking, orthogonal | keep |
| Stock cutoff constants 0.33/0.5 (B1) | **No** | keep |
| **G2.3 cutoff override duplicated across two `if` branches (B2, `:5785` + `:5807`)** | **Mild** — same setting read twice, same clamp twice, in the two arms of one `if/else` | **candidate to DRY** |
| Duplicated `LLCachedControl` static reads (llface `:1199-1200`, llvovolume `:5785-5786`, `:5807-5808`) | **Mild** — 3 independent reads of the same two keys | cosmetic |

The only real "too many pieces" is **cosmetic duplication of the settings reads and the cutoff
clamp** — not competing implementations. The architecture is already the intended
single-predicate/single-router design the patch log describes.

**What "use only the BD implementation" concretely means:** since BD == stock, it means *delete
G2.3 and lose the feature*. That is almost certainly **not** what the user wants (they built G2.3
on purpose). The actionable reading is "**collapse my own scattered pieces into one clean
path**," which is a light refactor, not a rip-out.

---

## 4. Consolidation plan

Two options. **Recommend Option A.**

### Option A (recommended) — keep G2.3, DRY the duplication. **Low risk, ~1-2 hrs.**

G2.3 is already the single-router design. Tighten it cosmetically without changing behavior:

1. **Collapse the two cutoff overrides (B2)** at `llvovolume.cpp:5785-5792` and `:5807-5813` into
   **one** post-`if/else` block. After the existing `if(mat){…}else{…}` that sets
   `mAlphaMaskCutoff`, add a single guarded override:
   ```
   if (force_mask && (type==PASS_ALPHA_MASK || type==PASS_FULLBRIGHT_ALPHA_MASK))
       draw_info->mAlphaMaskCutoff = llclamp((F32)force_cutoff, 0.f, 1.f);
   ```
   This is behavior-identical: the material branch's extra `mAlphaMode==BLEND` guard is redundant
   — a BLEND material face only *reaches* a mask pass when forced, so the pass-type test alone is
   sufficient. **Verify** that assumption before removing the guard (see risks).
2. **Optional:** factor the two settings into one tiny inline helper
   (`bdmergeForcedMask(bool& on, F32& cutoff)`) so llface and llvovolume read them one way. Pure
   tidiness; skip if it adds a header dependency.
3. Leave `canRenderAsMask()` A2 branch **exactly as is** — it is already the sole router.

**Do NOT touch:** the PBR early-out (`llface.cpp:1183`), the `mAlphaMode` PBR routing
(`llvovolume.cpp:6155`), the shadow render-type lists (`pipeline.cpp:11452/12669`), the draw-time
cutoff consumers (`lldrawpool.cpp:529/555/606`), or the non-alpha `DIFFUSE_ALPHA_MODE_MASK` path.

**Correctness risks (Option A):**
- **The redundant-guard removal (step 1)** is the only behavioral hazard. If any non-forced code
  path can put a BLEND-material face into a mask pass (it should not, per A1's `:1215`
  BLEND→false), removing the `mAlphaMode==BLEND` guard would apply the global cutoff to it. Safe
  because when `force_mask` is off the whole block is gated off; when on, only forced faces are in
  mask passes. Still: keep the change to a pure de-dup and confirm with a diff that the emitted
  cutoff value is unchanged for the off state (must be byte-identical to stock).
- Everything else is a no-op move; blast radius nil when `BDMergeForceAlphaMask` is off.

### Option B — "revert to BD/stock" (only if the user truly wants the feature gone).
Delete `llface.cpp:1193-1207`, `llvovolume.cpp:5782-5792`, `:5805-5813`, and the three
`settings.xml` keys (`:2917/2928/2939`). Result = byte-identical to BD/stock. **This discards
G2.3 entirely.** ~30 min. Only do this if the user confirms they want to abandon forced masking.

### What must NOT break (either option)
- **PBR/GLTF path** — non-BLEND PBR routes via `mAlphaMode` (`:6155`) and the `:1183` early-out
  keeps forced masking off PBR by design (documented scope cut). Preserve both.
- **Shadow-pass coherence** — masking must stay driven by batch membership so color+shadow agree;
  never add a second masking decision in the shadow path.
- **Rigged faces** — default-excluded; the `BDMergeForceAlphaMaskRigged` opt-in must keep riding
  `pushRiggedMaskBatches` (`lldrawpool.cpp:555`) with the same cutoff.
- **`force_mask` OFF == stock** — the off state must remain byte-identical to Linden/BD output
  (this is the whole safety contract). Verify by diffing emitted passes/cutoffs with the toggle off.

### Effort & risk summary
- **Option A (recommended): ~1-2 hrs, LOW risk** (cosmetic de-dup; one guard-removal to verify).
- **Option B (rip-out): ~30 min, LOW risk** but **loses the feature** — not recommended unless the
  user explicitly wants forced masking removed.

---

## 5. Direct answers to the mandate

- **How many masking layers / which are redundant:** One forced-masking implementation (G2.3),
  injected at 3 sites (1 router branch + 2 cutoff overrides), on top of the necessary stock
  pipeline (heuristic router → pass registration → per-batch cutoff → shadow reuse). No redundant
  *implementations*. Only mild cosmetic duplication: the two cutoff-override blocks and the
  repeated `LLCachedControl` reads.
- **Does a real BD forced-masking implementation exist:** **No.** BD's `canRenderAsMask()` is
  stock, BD has no forced setting/UI, BD's cutoffs are stock 0.33/0.5. Patch-log "donor gone"
  claim CONFIRMED.
- **What "strip Alchemy's, use BD's" resolves to:** BD == stock, so literally it means "delete
  G2.3." The sensible reading is "consolidate my own scattered pieces," which is Option A.
- **Recommended consolidation:** **Option A — keep G2.3, DRY the two cutoff overrides into one
  post-`if/else` block and optionally unify the settings reads. LOW risk, ~1-2 hrs.** Do not
  touch PBR routing, shadow lists, draw-time consumers, or the material-mask path.
- **Brief path:** `I:\alchemy-machinima\doc\BDMERGE_FORCED_MASKING_BRIEF.md`
