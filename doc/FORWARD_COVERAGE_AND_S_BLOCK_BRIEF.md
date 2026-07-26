# Forward-only scoping + the S block — DESIGN AND IMPLEMENT

**Repo `I:\alchemy-machinima`, branch `develop`, HEAD `a2bfceed82a`** (the sidecar + containment +
emissive-as-albedo fix just landed and is validated in-world).

Prior context, read these: `doc/ALBEDO_TEST_RESULTS_ROUND1.md` (raw in-world results, rounds 1-2),
`doc/ALBEDO_ROUND3_EMISSIVE_AS_ALBEDO.md` (round 3 — your own analysis confirming the fullbright
defect), `doc/RESHADE_BRIDGE_CONTRACT.md` (v2, authoritative), and
`doc/VISIBLE_DIFFUSE_SIDECAR_FIX_BATCH_BRIEF.md`.

## What just got fixed and confirmed in-world

`fullbrightF.glsl` no longer publishes unlit output as diffuse reflectance — it now writes
`vec4(vec3(0.0), final_alpha)`, the same "zero diffuse, known" statement the seed makes about metals.
**In-world result: the red candle bleed collapsed completely, and the usable RTGI Bounce Lighting
range moved back up from 0.250 to ~1.000.** Your round-3 first branch, confirmed.

## TASK 1 (primary) — forward-only scoping

**The error:** `PS_ProvideAlbedo` selects the sidecar wherever `K >= threshold`. K is 1 for BOTH the
deferred-opaque seed AND forward-opaque surfaces, so no threshold can separate them, and we supersede
albedo at every pixel — including the deferred-opaque pixels where the G-buffer answer was already
correct and needed no help. The feature exists ONLY to repair forward-rendered pixels (alpha, water,
fullbright), which never write the G-buffer.

**The plumbing already exists and is HALF-BUILT. Verify all of this yourself:**

- `indra/newview/llreshadebridgeabi.h:207` — `SLRESHADE_SEM_VALID_SURFACE_COVERAGE (1u << 8)`
- `indra/newview/llreshadebridgeabi.h:275` — `SLReShadeTexture surface_coverage;  /* words 12..15 */`
- `indra/newview/llreshadebridgeabi.h:259` — documented as **GL_RG8: R = deferred/G-buffer visible
  coverage [0,1]**, G currently unspecified/unused.
- `reshade-addon/shaders/SL_Bridge.fxh:47` — `texture SLSurfaceCoverageTex : SL_SURFACE_COVERAGE;`
  with the comment **"R: G-buffer coverage, G: forward coverage"** — the intent is already recorded.
- `reshade-addon/shaders/SL_Bridge.fxh:69` — the `SL_sSurfaceCoverage` sampler.
- `reshade-addon/shaders/SL_Bridge.fxh:186` — `float2 SL_SurfaceCoverage(float2 uv)` returns `.rg`.
- `reshade-addon/shaders/SL_Bridge.fxh:119` — `SL_SEM_VALID_SURFACE_COVERAGE 0x100u`
- `reshade-addon/src/sl_reshade_bridge.cpp:134` — `GL_RG8 -> r8g8_unorm` mapping exists.
- `reshade-addon/src/sl_reshade_bridge.cpp:360` — `case SRC_SURFACE_COVERAGE:` slot exists.

**THE GAP: `indra/newview/llreshadebridge.cpp` publishes NOTHING for this semantic.** No target is
allocated, nothing is written, the tail texture is never filled, the validity bit is never set.
So `SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE)` is always false and every gate that depends on it is
dead code today.

**Design and implement the whole path, both sides:**

1. Viewer: allocate the RG8 coverage target. Decide where — a second attachment, a separate target,
   or reuse. Justify against the hazards below.
2. Viewer: **clear `.g` to zero every frame** on the main view.
3. Viewer: have forward diffuse-bearing draws write/blend their visible coverage into `.g`. Determine
   exactly which draws — this must match the set that writes `visible_diffuse`, or the gate and the
   data disagree. Note `fullbrightF.glsl` now writes RGB=0/K=coverage: **decide whether fullbright
   should register forward coverage at all.** If it does, the consumer will select a black sidecar
   value over Launchpad's estimate at those pixels — argue whether that is right.
4. Viewer: also populate `.r` (deferred/G-buffer coverage) if cheap, or state why not and leave it
   defined.
5. Viewer: fill `tail.surface_coverage` and set the validity bit — **with a positive production
   latch, exactly like the two-stage `mPendingVisibleDiffuseSeeded`/`Resolved` pattern already in
   `llreshadebridge.cpp`. Never infer validity from texture existence** (contract v2 §4).
6. FX: gate the sidecar branch. Your proposed gate was:

```hlsl
if (SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE) &&
    SL_SemValid(SL_SEM_VALID_SURFACE_COVERAGE) &&
    SL_SurfaceCoverage(uv).g >= 0.5)
{
    float4 vd = SL_VisibleDiffuse(uv);
    if (vd.a >= max(SL_VISDIFF_K_THRESHOLD, SL_VISDIFF_K_MIN))
    {
        o = vd.rgb;
        return;
    }
}
```

   **Refine it.** In particular: what happens when VISIBLE_DIFFUSE is valid but SURFACE_COVERAGE is
   NOT (old viewer, or the new path failed)? Falling back to "sidecar everywhere" reintroduces
   today's bug; falling back to "sidecar nowhere" silently disables the feature. Pick one, justify
   it, and make the choice observable.
7. FX: partial coverage. `.g` is 8-bit UNORM and a semi-transparent forward layer has fractional
   coverage. Is `>= 0.5` right, or should it LERP between sidecar and incumbent by `.g`? Note the
   sidecar's own K already encodes accumulated exactness — say whether `.g` and K are independent or
   redundant, and do not double-apply them.
8. Debug: add a `surface_coverage` view to `SL_BridgeDebug.fx` so this is verifiable in-world. It
   MUST distinguish "semantic invalid" from "valid but zero" — reuse the existing
   `sl_invalid_pattern` magenta/black stripes convention. Also update the 12-cell status strip if
   cell assignment needs it.

**Hazards this must respect** (all documented in the contract and re-verified this session):
- H1 shaders declaring only `frag_color` write UNDEFINED to extra attachments.
- H2 `LLRender::setColorMask` is global and resets indexed state.
- H3 `gGL.blendFunc` is global AND caches/skips.
- H4 `renderGeomPostDeferred` has other callers — gate on target identity.
- H5 additive faces are emitters and must not overwrite.
- The `LLRender` indexed guard added this session (`setIndexedDrawBufferGuard`,
  `setIndexedDrawBufferGuardMask`, `...Blend`, `LLScopedIndexedDrawBufferGuard` in
  `indra/llrender/llrender.h`) is **single-owner and asserts if armed twice.** If the coverage target
  needs its own indexed state, say how that coexists — extend the guard to a stack, use a separate
  attachment index, or use a dedicated pass.
- The clear colour is `glClearColor(1,0,1,1)` and `bindTarget` clears ALL attachments, so an
  unwritten channel is NOT zero. Do not assume it is.

## TASK 2 — S3, water is a hole

`waterF.glsl` was never in the modified set, so water publishes **seabed albedo at K=1** — the surface
behind the water, asserted as a known answer. Confidently wrong, which is worse than a gap. The
contract says water is "zero diffuse with K=1, a BRDF statement not a gap"
(`waterF.glsl:288,303` referenced in the contract). Implement it, and state whether water should
register forward coverage.

## TASK 3 — S2 and S4, fullbright K accumulation

Adjacent to the fix just landed. RGB is now correct (zero); K is not.
- **S2** fullbright K under-accumulates non-deterministically.
- **S4** fullbright-alpha-mask writes `K = final_alpha` on opaque pixels — on an opaque alpha-mask
  pixel `final_alpha` may not be 1, so a fully-known pixel is published as partially known.
Diagnose both against the current code and fix.

## TASK 4 — S6, minor

The reset-event counter is a stealth field in `tail.reserved[0]` while `llreshadebridgeabi.h` still
describes `reserved` as "future growth". Either document it properly in the ABI as a named field with
a version note, or move it. Do not break the existing layout without saying so loudly.

## WHAT TO PRODUCE

**Read the actual code on BOTH sides. The user explicitly asked that the .fx shader code be included
— do not produce viewer-side C++ only.**

Consumer side to read and modify:
- `reshade-addon/shaders/SL_GBufferProvider.fx` — `PS_ProvideAlbedo`, the technique/pass list,
  `RenderTargetWriteMask`s, every `#define` block.
- `reshade-addon/shaders/SL_Bridge.fxh` — accessors, samplers, semantic bits, `SL_UV`.
- `reshade-addon/shaders/SL_BridgeDebug.fx` — `sl_k_heat`, `sl_invalid_pattern`, `PS_TailStatus`,
  the view enum.
- `reshade-addon/src/sl_reshade_bridge.cpp` — `update_slot`, `fail_slot_closed`, the semantic table,
  `map_gl_internal_format`.

Producer side:
- `indra/newview/llreshadebridge.cpp` / `.h` / `llreshadebridgeabi.h`
- `indra/newview/pipeline.cpp` — allocation, `renderGeomPostDeferred`, the seed dispatch
- `indra/newview/lldrawpoolalpha.cpp`, `lldrawpoolsimple.cpp`
- `indra/newview/app_settings/shaders/class1/deferred/fullbrightF.glsl`,
  `class2/deferred/alphaF.glsl`, `class2/deferred/pbralphaF.glsl`,
  `class1/deferred/visibleDiffuseSeedF.glsl`, and the water shader
- `indra/llrender/llrender.h` / `.cpp` — the indexed guard

Output **complete, mechanically applicable code** — unified diffs or fully annotated blocks with
exact file paths and anchor lines — for BOTH the .fx/.fxh consumer side and the viewer side.
**The workspace may be read-only for you: put everything in your FINAL MESSAGE as text, do not try
to write files.** A previous run wasted a turn discovering this.

Also state:
- Whether any of TASK 1's design forces an ABI version bump, and if so what breaks.
- The order to implement in, and which parts can ship independently.
- What is verifiable in-world at each step, with the debug view to use and what a PASS looks like.
  Claude cannot run the viewer; the user does. Every diagnostic must state a verdict —
  PASS / FAIL / INCONCLUSIVE as distinct outcomes — and must never silently no-op into a confident
  wrong answer.
- Anything in the brief you think is wrong. Two hypotheses were confidently wrong tonight before
  in-world evidence corrected them.
