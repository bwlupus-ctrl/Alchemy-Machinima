# Fable Adversarial Review — the six MERGE_*.md docs

**On disk:** `I:\alchemy-machinima\doc\MERGE_DOCS_FABLE_REVIEW.md`
**Reviewed:** 2026-08-10, 4 Fable reviewers, each cross-checking against upstream
`alchemy-upstream/develop @ af0f3bd1beb`, merge-base `7c11d3f38bd`, and fork `develop @ 47af3df7b1a`.
Consolidated + de-duplicated + adjudicated by Claude (Opus).

## Overall verdict
The docs are **substantially sound and line-exact** — all four reviewers independently concluded
they were produced against the real trees, not confabulated. `MERGE_PIPELINE_PLAN`,
`MERGE_CONTRACT_DIFF`, `MERGE_SHADER_DRIFT`, and `MERGE_CLONES_ADAPTATION` are trustworthy
implementation references needing only small fixes. **Two areas need real work before execution:**
1. **`MERGE_PLAYBOOK.md`'s execution model** — 1 blocker + 4 majors (endgame git mechanics, re-port
   scope, dependency order, intermediate-build viability, rollback realism).
2. **Stash-vs-committed conflation** in the VCam/cookie material — the plan references reverted WIP
   as if committed, and mis-specifies the cookie contract.

Ship-readiness of the *plan*: **not yet** — fix the 2 blockers + 6 majors below, then it's executable.

---

## BLOCKERS (fix before executing the merge)

### B1 — Playbook endgame is the giant merge it rejects  *(Playbook, F1)*
"Definition of done" says finish by merging the integration branch into develop. The integration
branch starts at `af0f3bd` whose merge-base with develop is still `7c11d3f38bd`, so `git merge` =
the Option-A conflict blob the doc explicitly rejects. **Fix:** advance the `develop` ref to the
integration head (branch reset / force-move) with develop's old head kept in a backup ref; document
the exact mechanics. (Verified: `git merge-base develop alchemy-upstream/develop` = 7c11d3f.)

### B2 — Playbook re-port scope silently drops ~70 features  *(Playbook, F2)*
Steps name only ~6 feature files; the fork added **326 files / 73+ .cpp** under `indra/newview`
since the base (ghost-studio suite, director/anim-switcher, temporal capture, flowgrid, path editor,
object-path mover, weather model, radar port, the `llviewermenu`/`llagent`/`llviewerwindow` hook
edits, etc.). Under B1's ref-advance, everything unassigned to a step **vanishes**. **Fix:** add an
application-layer landing step with a file-inventory ledger (`git diff --diff-filter=A 7c11d3f..develop`)
— "no fork file silently omitted," mirroring the pipeline hunk ledger.

---

## MAJORS

### M1 — Stash-vs-committed conflation + wrong cookie contract  *(cross-cutting: Contracts F1, VCam M1/M3/m4)*
The most important systemic issue. Multiple docs reference **reverted WIP as committed fork surface**:
- **Projector cookie remap** (`proj_cookie_region/orient`, `projCookieUv`) has **zero hits** at
  `47af3df7b1a`; it lives only in `stash@{0}` ("vcam-projector-wip-20260808", `80769c19564`) — the
  stash reverted **because of the known main-cam-moves-feed-shadow regression**. Worse, the doc's
  spec is **type-wrong** vs. the real stashed code: `proj_cookie_orient` is a **`vec4` affine
  scale/offset term** (doc says `int` swap-flag), and `proj_cookie_region` packs **xy=scale, zw=offset**
  (doc says the reverse). A porter following the doc would mis-crop every feed cookie.
- **`alprismcamdriver.*`** is stash-only, not on develop.
- **`llcinematiccamera` names** `computeOutputFrame` / `writeMainCamera` / `writeVirtualCameraOutput`
  are **hallucinated** (zero hits repo-wide). Real surface: `updateCamera`, `applyFrameLens`,
  `resolveAnchor`, `captureCurrentSwitcherView`, `pattern*()`.

**Fix:** re-base every doc on committed `47af3df7b1a`; label each stash-sourced piece as WIP
(recoverable from `stash@{0}`/`80769c19564`) with its known regression; re-derive the cookie contract
from the real stashed code (two vec4 scale/offset terms, orient→region order) or mark it
"to be redefined at re-land"; correct the cinematic names.

### M2 — VCam aux-feed SSR containment silently breaks  *(VCam, M2)*
Upstream moved the SSR march params **into `ReflectionProbeData`** (the probe UBO struct). So the
fork's per-bind `uniform1f(DEFERRED_SSR_ITR_COUNT, 0)` for the aux feed becomes a **no-op** (block
member, location −1), and **`reflectionProbeF.glsl` is missing from the doc's shader-edit map** — so
nothing replays the fork's `prism_auxiliary` guards onto upstream's rewritten SH shader. Result: the
aux feed runs a full SSR march against an unbound scene map (upstream installs a white placeholder) →
bright/garbage reflections in the feed. **Fix:** zero `iterationCount` and `glossySampleCount` in the
staged `prism_probe_data` (trivial now that SSR rides in the staged struct), and add
`reflectionProbeF.glsl` to the replay list with the `prism_auxiliary` guards re-expressed against the
SH version. (RAII restoration on the way *out* is verified correct; the gap is containment on the way *in*.)

### M3 — Playbook dependency inversion  *(Playbook, F3)*
Fork `pipeline.cpp` `#include`s `llprismlens.h` and calls `LLPrismLens::MAX_CAPTURES`,
`onRenderTargetsReleased`, `getActiveClipPlane`, `getAuxCompositeStates`. The early steps (3–6)
cannot compile until `llprismlens.h/.cpp` (Step 7) land. **Fix:** land the VCam headers (or a
constants/interface shim) in Step 3, or split interface from implementation.

### M4 — Playbook intermediate builds are non-viable  *(Playbook, F4)*
Fork `llviewershadermgr` registers `gPrismLensProgram` / `gActorGhostProgram` **unconditionally**
(`createShader()` at :1378-1384, :4352). Every Release build between the registration step and the
GLSL step fails shader load at startup. "Build Release after each checkpoint" is hollow until the
shaders co-land. **Fix:** co-land each program's adapted GLSL in the same commit as its registration,
or gate registration behind a feature flag until the shader step.

### M5 — Playbook rollback is over-optimistic  *(Playbook, F5)*
Because of M4, the first functionally testable build is post-shader-step, so a defect introduced in
an early step surfaces much later, where `git revert <early-commit>` collides with later commits in
the same pipeline.cpp regions. **Fix:** state that early gates are static-only; late-found regressions
are fixed forward or rolled back to a checkpoint *branch*, not clean single-step reverts.

---

## MINORS (fix in a cleanup pass)

- **finalizeShaderList** *(Contracts F3, Clones 3)* — upstream **keeps** an assert-shell
  `finalizeShaderList()` (`:473`, called `:754`); only the fork's ~100-line `mShaderList` body is gone.
  "Remove finalizeShaderList" as written breaks the upstream call site. Rephrase to "keep the shell,
  drop the fork body, register with a unique program name."
- **ensurePrismLensOutput → allocatePrismLensOutput** *(Contracts F2, VCam m1)* — real name at
  `pipeline.cpp:1456` (companions `releasePrismLensOutput`). Snippet body is verbatim-real, just mislabeled.
- **Clones rigged normal transform** *(Clones 1)* — doc uses `normal_matrix * skinDirection(...)`;
  all 26 upstream rigged consumers use `mat3(modelview_matrix) * skinDirection(...)`. Renders OK
  (normalize hides it) but off-convention — match upstream.
- **mAvatarDepth field ownership** *(Playbook F6)* — the interleaved walk consumes `mAvatarDepth`
  (fork `llspatialpartition.h:475/590`), a field **no step owns** (upstream has `mAvatarp`/`mRenderOrder`
  but not `mAvatarDepth`). PIPELINE_PLAN and PLAYBOOK disagree on who lands the bridge stamps; make
  them agree and note interleaving is inert until the stamp step.
- **PIPELINE_PLAN P1** — anchor `:12344` is `renderWeather` (P8), not projvol. (Other two P7 anchors exact.)
- **PIPELINE_PLAN P2/P3** — `ScopedPrismAuxiliaryState` and the `getSpotShadowTarget` patch already
  exist in the fork (`ScopedPrismRenderState` :4796; getSpotShadowTarget :18713). Present as "adapt
  existing," not "write new."
- **VCam m2** — §3 says feed shadows use `mainDepthFormat()`; upstream shadow policy is
  `(AlchemyRenderShadowDepth32F || sReverseZ) ? DEPTH_FMT_32F : DEPTH_FMT_24`. Fix the §3 sentence.
- **VCam m3** — `llprismlens.cpp:5210` is a `getTexUnit(0)->unbind()`; translation is
  `getTextureSlot(0)->unbind()`, no named sampler involved.
- **VCam m5** — `prismLensV.glsl` declares loose matrix uniforms; upstream matrices ride the
  `UB_MATRICES` engine block. Delete the loose decls, add the `//[ENGINE_BLOCK Matrices]` injection.
- **Clones 2** — the `!sImpostorRender` gate on `canUseInterleavedAlpha()` is a **bake-time behavior
  change** (the fork currently runs the merged iterator inside impostor bakes), not a neutral flag add.
  Correct plan, but the transition test is load-bearing.
- **Clones 4** — no `gSkinnedActorGhostProgram` header decl to delete (it's cpp-local).
- **Grounding doc SHA typo** *(Contracts F4)* — `UPSTREAM_ALCHEMY_MERGE_DEEP_RESEARCH_GPT.md` §T1:
  `78eff87b09b` → real `78eff87a60b`. (All other 29 SHAs resolve.)
- **Grounding doc commit count** *(Playbook)* — "325" → measured **327**/255.
- **Hardening** — `MERGE_SHADER_DRIFT`'s `AL_SHADOW_SAMPLER` uses `#ifdef SHADOW_PCSS`; upstream's
  convention is value-based (`#define SHADOW_PCSS 0` default). Use `#if SHADOW_PCSS`.
- **Line-anchor drift** — a handful of cites off by a few lines (mask-cutoff block, resolve_outer_transform,
  applyModelMatrix); all resolve, cosmetic.

---

## Verified-SOLID (attacked and held)
Reviewers tried to falsify and could not: the immutable-RT `allocate()` 8-arg signature + `resize()`
contract; `ALTextureSlot`/`getTextureSlot()`/named samplers (`ALSamplers::*`); `ReflectionProbeData`
+ `ALUniformBuffer::update/allocated`; SH staging idiom + RAII restoration; reverse-Z bias math and
the deliberately-forward cookie projection; PCF/PCSS `bindShadowMaps` selection; the clone
outer-transform composition under avatar-local skinning; the palette split-upload
(`&mGLMp[4]`/`&mGLMp[0]`) + `MatrixPaletteCache` contiguity; impostor `sImpostorRenderAlphaDepthPass`
deletion + "stamp between deferred and forward"; `llvoavatar` churn map (idleUpdateMisc /
updateRootPositionAndRotation = zero hunks; the rest profiling-only or as-claimed);
`llcontrolavatar` zero upstream churn; the DROP-OBSOLETE targets all really gone upstream. The
validation checklist covers every committed regression sentinel.

---

## Recommended next action
Hand this file back to GPT to revise the six docs to **0 blockers / 0 majors**, in this order:
1. Re-base all docs on committed `47af3df7b1a`; quarantine stash-sourced material (M1) with a clear
   "WIP / recover from stash@{0}, has regression X" banner.
2. Rewrite `MERGE_PLAYBOOK.md`'s execution model (B1, B2, M3, M4, M5) + the file-inventory ledger.
3. Fold M2's SSR-containment fix + `reflectionProbeF.glsl` into `MERGE_VCAM_ADAPTATION.md`.
4. Sweep the minors (names, `finalizeShaderList`, normal transform, anchors, SHA/commit-count typos).
Then a second short Fable/Codex pass to confirm 0 major before any code is touched.
