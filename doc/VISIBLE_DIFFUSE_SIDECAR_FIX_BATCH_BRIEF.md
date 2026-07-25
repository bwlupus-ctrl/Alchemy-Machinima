# Visible-diffuse sidecar — BATCHED FIX BRIEF (M1-M6, S1, S5, FX1-FX4)

**Repo:** `I:\alchemy-machinima`, branch `develop`, HEAD `90b9c8e216d`.
**All sidecar work is UNCOMMITTED in the working tree.** Use `git diff` / `git status` to see it.

**Feature setting:** `RenderVisibleDiffuseSidecar` (default OFF, currently OFF in user settings).
The feature is disabled because with it ON the published sidecar is undefined garbage every frame,
flagged VALID.

## What the feature is

The ReShade bridge feeds the viewer G-buffer to iMMERSE RTGI. Albedo is wrong at forward-rendered
pixels (alpha, water, fullbright) because those never write the G-buffer — the albedo attachment
holds the surface *behind* them. The sidecar is a **second colour attachment on `mRT->screen`**
(`GL_SRGB8_ALPHA8`, added at `pipeline.cpp:1078`) capturing visible-surface diffuse in the same draw
calls as the forward beauty pass.

- **RGB = linear diffuse, A = exactness K** (1 = known answer, 0 = no answer).
- ABI 1.2, `visible_diffuse` at tail words 20..23, `SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE` = bit 9.
- Seed shader `class1/deferred/visibleDiffuseSeedF.glsl` classifies (it is NOT a blit — a raw blit
  inverts exactness, because deferred opaque writes .a = 0.0 and sky writes .a = 1.0).

## Verified-good, do not "fix"

- The seed shader itself is correct: uniforms, windowed compare, sky excluded, K semantics,
  `orm.b` IS metallic, sRGB round-trip sound.
- The gate-OFF path is byte-identical to stock. Keep it that way.
- ABI layout is correct, the tail fits exactly, and it fails closed against 1.0/1.1 writers.
- **Sky albedo is BLACK.** `RenderEnableEmissiveBuffer` defaults to ON (`settings.xml:12592`), so sky
  writes `frag_data[0] = vec4(0)`. The radiance-to-attachment-0 branch (`skyF.glsl:220`) is the
  NON-default path. Multiple earlier research passes asserted the opposite and were wrong.
- **A `.w > 0` coverage gate is UNSOUND.** Clear is `glClearColor(1,0,1,1)`
  (`llviewerdisplay.cpp:1020`) and `bindTarget` clears ALL attachments, so unwritten `.w` = 1.0,
  colliding with HAS_HDRI. Use the windowed two-bucket test
  `abs(w-0.34)<0.1 || abs(w-0.67)<0.1`. Never equality, never `> 0`.
- Metals and water = zero diffuse with K=1. That is a BRDF statement, not a gap.

## The five structural hazards

- **H1** Shaders declaring only `frag_color` write UNDEFINED values to attachment 1 when it is
  unmasked.
- **H2** `LLRender::setColorMask` issues a **global** `glColorMask`, which resets every indexed mask.
- **H3** `gGL.blendFunc` is non-indexed AND **caches/skips the GL call when factors are unchanged**
  (`llrender.cpp:1375-1384`), so indexed blend state dies non-deterministically by batch order.
- **H4** `renderGeomPostDeferred` has other callers (impostors / HUD / probe) — gate on target
  identity via `LLRenderTarget::getCurrentBoundTarget()` (`llrendertarget.h:197`). There is no
  `LLPipeline::sBoundTarget`.
- **H5** Additive faces are emitters and must not overwrite the sidecar.

**The structural lesson from review, verbatim:** per-pass mask asserts in the pipeline loop are
*structurally insufficient* against pools and helpers that issue global mask/blend calls mid-pass.
Either route those through an indexed-aware wrapper while the sidecar is live, **or stop sharing the
FBO** and seed/resolve the sidecar in dedicated passes.

---

# MUST-FIX — viewer side

## M1 — `doAtmospherics` / `doWaterHaze` wipe the entire sidecar every frame (THE root cause)

`pipeline.cpp:14108` and `pipeline.cpp:14160`. Both do `gGL.setColorMask(true, true)` (global, H2 —
re-enables the indexed mask that the pool loop just cleared) and then draw a **fullscreen triangle**
with `gHazeProgram` / `gHazeWaterProgram`, whose shaders declare only `frag_color` → **undefined
value at every pixel of attachment 1** (H1).

Invoked mid-pool-loop at `pipeline.cpp:6191` and `pipeline.cpp:6197`. Deterministic, every frame,
whenever `RenderDeferredAtmospheric` is on. **The beauty buffer is untouched, so it is perfectly
silent.** This alone explains both K anomalies seen in-world (yellow 0<K<1 on solid opaque geometry,
and deep blue K=0 on large solid architecture).

Note `doWaterExclusionMask()` at `pipeline.cpp:6185`, and `renderHighlights()` / `renderDebug()` at
`pipeline.cpp:6284-6287`, are in the same class and must be considered.

## M2 — glow pool leaves attachment 1 undefined

`lldrawpoolsimple.cpp:53`: `LLDrawPoolGlow::renderPostDeferred` does `gGL.setColorMask(false, true)`
(global) then draws emissive programs that declare only `frag_color`. Undefined K at every glowing
object.

## M3 — alpha-pool emissive sub-passes: garbage K **and RGB NaN-poisoning**

`lldrawpoolalpha.cpp:897-925`. The alpha pool unmasks attachment 1 for the whole pass
(`lldrawpoolalpha.cpp:590-593`), then the emissive sub-passes issue a **global**
`gGL.blendFunc(BF_ZERO, BF_ONE, BF_ONE, BF_ONE)` (line 897) and run single-output shaders
(`renderEmissives`, `renderPbrEmissives`, `renderRiggedEmissives`, `renderRiggedPbrEmissives`).
Attachment 1 is still write-enabled → undefined writes, and with an undefined source
`NaN * 0 = NaN`, so the RGB is poisoned too, not just K.

## M4 — publication infers validity from texture existence

`llreshadebridge.cpp:299-301` sets `SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE` from
`texture_present(tail.visible_diffuse)` alone. This is exactly the contract §4 violation that the
validity bit exists to prevent. **MOTION does it correctly two lines above**
(`texture_present(f.motion) && mPendingMotionCoverage != 0`) — follow that precedent.

Additionally: **there is no settings listener** for `RenderVisibleDiffuseSidecar`, so
toggle-then-resize publishes garbage flagged valid. Compare how other render settings register a
listener that forces a target rebuild.

## M5 — the CTD is still reachable through the loader

`llviewershadermgr.cpp:2390-2408` folds this OPTIONAL program into the load chain via
`success = gVisibleDiffuseSeedProgram.createShader();` followed by `llassert(success);` — **before**
`gDeferredSoftenProgram` is created. A seed-shader compile failure on any other driver therefore
kills every subsequent program in the chain, reproducing the original
`ERROR llglslshader.cpp(987) bind : ASSERT (mProgramObject != 0)` crash one shader over. The
dispatch-site `isComplete()` guard fixed the symptom; this loader coupling reintroduces it.

**Rule: an optional program must never gate `success`, and must never `llassert` on its own
compilation.** It must degrade to feature-off.

## M6 — breaks the ALREADY-COMMITTED velocity work, not just the sidecar

`pipeline.cpp:14074-14079`. The `else { sHasLastVelocityProjection = false; }` runs on **every**
`gCubeSnapshot` pass, and reflection probes update continuously, so
`SLRESHADE_RESET_PROJECTION_CHANGE` is published **every frame**. Temporal consumers reset
permanently → TAA / denoising silently dead. The `else` protects nothing. Delete it.

**M6 is independent of the sidecar and should be separable.**

## S1 — probes are not excluded (promoted, it is part of doing M1 right)

`mRT` follows the probe swap, so aux / hero targets pass the `getCurrentBoundTarget() == &mRT->screen`
identity test and get sidecar attachments. Exclude `gCubeSnapshot` / probe and HUD renders explicitly.

## S5 — no GL capability gate (promoted: it is a CTD)

`glBlendFuncSeparatei` requires GL 4.0 and `glColorMaski` requires GL 3.0. Both are loaded as
function pointers in `llgl.cpp` (`glColorMaski` at :1884, `glBlendFuncSeparatei` at :2055) and will
be **null** on an older context → crash. The feature must hard-disable itself when either is
unavailable, at target-allocation time, not at draw time.

---

# MUST-FIX — FX / add-on side (Codex's own earlier review)

## FX1 — bind-before-destroy violation

`reshade-addon/src/sl_reshade_bridge.cpp:676`. The recreation path calls `slot.destroy(dev)` while
the semantic is **still bound to `slot.dest_srv`**; the neutral rebind does not happen until
`update_texture_bindings` at :718. Every early-return failure path between those two points leaves
ReShade holding a destroyed SRV. Must route through `fail_slot_closed()` (neutral-bind first) before
destroying.

## FX2 — K=0 ("no answer") takes the sidecar path

`reshade-addon/shaders/SL_GBufferProvider.fx:357` declares `SL_VISDIFF_K_THRESHOLD` with
`ui_min = 0.0`, and :450 tests `if (vd.a >= SL_VISDIFF_K_THRESHOLD)`. At threshold 0 every
"no answer" pixel becomes authoritative. Require `K > 0`, or floor the threshold at one UNORM step
(1/255). Default 0.5 is fine and should stay.

## FX3 — `RenderTargetWriteMask = 3` is no longer unconditional  ⚠️ SCOPE DECISION PENDING

`reshade-addon/shaders/SL_GBufferProvider.fx:565-575`. The mask is now inside `#if !SL_MODE_OWNED`,
so OWNED writes all four channels and can overwrite Launchpad's `.zw` geometry normal (clobbering
that previously caused sparkle). The in-code comment argues OWNED must define `.zw` itself, which is
defensible on its own terms — the objection is **scope**: this is a normals-ownership change that
rode in on an albedo feature.

**DO NOT CHANGE FX3 IN THIS BATCH.** Analyse it and state a recommendation only.

## FX4 — the "byte-identical fallback" claim is false

`reshade-addon/shaders/SL_GBufferProvider.fx:437-456`. `PS_ProvideAlbedo` was genuinely
restructured; the sidecar branch now sits ahead of every pre-existing gate. The fallback is
*behaviourally* equivalent only when the semantic is invalid. Either restore a genuinely
byte-identical path when the semantic is invalid, or correct the claim and justify the restructure.

---

# EXPLICITLY DEFERRED (do not fix in this batch; note if you disagree)

- **S2** fullbright K under-accumulates non-deterministically.
- **S3** water publishes seabed albedo at K=1 instead of known-black. (Water is currently a HOLE —
  `waterF.glsl` was outside the modified set and gets no forward sidecar write at all.)
- **S4** fullbright-alpha-mask writes K = final_alpha on opaque pixels.
- **S6** the reset counter is a stealth field in `reserved[0]` while the header still says
  "future growth".

---

# WHAT TO PRODUCE

Produce a **complete independent implementation** of M1-M6, S1, S5, FX1, FX2, FX4.

**⚠️ DO NOT MODIFY ANY SOURCE FILE.** Claude is implementing the same brief in the working tree in
parallel and the two implementations will be diffed against each other adversarially. Writing to
the sources would clobber that work.

**Write your implementation to a NEW file: `doc/CODEX_SIDECAR_FIX_IMPLEMENTATION.md`.** Use unified
diffs or full annotated code blocks with exact file paths and anchor lines, enough that it could be
applied mechanically.

Answer these design questions explicitly, because they are where the two implementations are most
likely to diverge:

1. **Containment strategy.** Reassert indexed state centrally (wrap `LLRender::setColorMask` and
   `LLRender::blendFunc` so any global call restores the live indexed-1 state), OR make
   `LLRenderTarget` own the indexed state and reassert after every bind, OR stop sharing the FBO and
   resolve the sidecar in a dedicated pass. GPT's external design decision #8 favoured the
   `LLRenderTarget`-owned variant. **State which you chose and why the other two are worse.**
   In particular: does a central wrapper actually close H3 given that `blendFunc` caches and skips?
2. Does the central-wrapper approach still leave holes for code paths that call raw `glColorMask` /
   `glBlendFunc` directly rather than going through `gGL`? Enumerate any you find.
3. For M3, is masking attachment 1 off around the emissive sub-passes sufficient, or does the
   NaN-poisoning require something stronger?
4. For M4, what is the correct positive validity predicate? The sidecar has no direct analogue of
   `mPendingMotionCoverage`. Should one be introduced (a per-frame "the seed pass ran and the pool
   loop completed on the main view" latch)? GPT's external decision #7 proposed a readiness latch.
5. For M5, what is the correct idiom in this codebase for an optional program that must not gate
   `success`? Cite an existing precedent in `llviewershadermgr.cpp` if one exists.
6. Is M6 genuinely separable into its own commit ahead of everything else?

Also: **actively look for anything the review missed.** State clearly if you think any of M1-M6 is
misdiagnosed — the earlier "H3 confirmed" call in this same investigation was itself an incomplete
diagnosis that a second review overturned, so do not assume the writeup above is correct.

## Build / test context

- Build: `cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release`
- The running viewer holds a link lock; it must be closed before a rebuild.
- Claude cannot run the viewer or log in — the user does. So any diagnostic must make the LOG STATE
  A VERDICT: PASS / FAIL / INCONCLUSIVE as distinct outcomes. A diagnostic must never silently
  no-op into a confident wrong answer.
- In-world debug view: `SL_BridgeDebug` has an "Exactness K" mode and a "Visible diffuse" mode.
  Green = K 1, yellow = 0<K<1, deep blue = K 0, sky-blue at the top of frame is correct.
