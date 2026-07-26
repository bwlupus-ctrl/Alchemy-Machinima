# FULL-SESSION AUDIT — review EVERY change made tonight as one body of work

**A regression escaped per-change review. The user has asked for the whole picture to be audited
together, not change by change.** Assume there are more.

## Why this audit exists

You recommended adding `LLGLEnable(GL_BLEND)` to `LLDrawPoolFullbright::renderPostDeferred` for S2.
I implemented it, it passed your per-change review, and **the user reported a visible in-world
regression within minutes**: opaque fullbright surfaces whose textures carry an alpha channel went
see-through, because the beauty pass was relying on inheriting blend-DISABLED.

**The failure mode that got through: a change made for the SIDECAR altered state the BEAUTY PASS
depends on.** Every per-change review reasoned about the sidecar correctly and missed the beauty
pass. Audit specifically for more of that class.

Reverted, with a `⛔ DO NOT` comment at the site. Replaced with your shader-side fix
(`#if defined(HAS_ALPHA_MASK) || !defined(IS_ALPHA)`), which is applied but **not yet built or
tested in-world**.

## The complete change set

Four commits, plus uncommitted work. Read the full diffs:

```
c8983659a66  S6: name the reset-event counter instead of hiding it in reserved[0]
6ca9b2f065b  ReShade bridge: forward-only scoping, plus S2/S3/S4
7f092fcc84f  Film clean: suppress viewer effect particles while the UI is hidden
a2bfceed82a  ReShade bridge: visible-diffuse sidecar, containment fixes, emissive-as-albedo
```

Base: `git diff 90b9c8e216d..HEAD` plus `git diff` for the uncommitted S2 revert + shader fix.

### What is in them, grouped by risk

**A. Global/shared GL state — HIGHEST RISK, this is where the regression was**
- `LLRender::setColorMask` and BOTH `blendFunc` overloads now call `reassertIndexedDrawBuffer()`
  inside the state-changed branch (`indra/llrender/llrender.cpp`).
- New indexed draw-buffer guard holding an ARRAY of attachment states under one owner, with an
  `llassert(!mIndexedGuardActive)` single-owner rule (`llrender.h/.cpp`).
- `llscenemonitor.cpp`: two raw `glColorMask` calls converted to `gGL.setColorMask`.
- `lldrawpoolsimple.cpp`: the reverted `LLGLEnable(GL_BLEND)`.
- **AUDIT: does the reassert-inside-blendFunc/setColorMask change ANY beauty-pass behaviour on the
  feature-OFF path? It must be a strict no-op when the guard is unarmed. Prove it.**
- **AUDIT: the scene-monitor conversion changes when the mask actually reaches the driver, because
  gGL caches. Can that alter the scene monitor's own output or anything after it?**

**B. Render-target layout**
- `mRT->screen` gains attachment 1 (`GL_SRGB8_ALPHA8`) and attachment 2 (`GL_RG8`), main view only,
  behind `RenderVisibleDiffuseSidecar` (`pipeline.cpp`).
- **AUDIT: does adding attachments change `glDrawBuffers`, clear cost, or any code that assumes
  `getNumTextures() == 1` on `screen`? Search for assumptions about that target's attachment count,
  including in snapshot, HUD, impostor and probe paths.**

**C. Pool classification and per-pass state**
- `POOL_WATER` added to `contributes_diffuse` in `renderGeomPostDeferred`.
- Alpha pool opens/closes indexed masks and blends around draws and the emissive sub-passes.
- **AUDIT: water now runs with the guard OPEN. `LLDrawPoolWater::renderPostDeferred` disables
  GL_BLEND — confirm that combination cannot change attachment 0.**

**D. Shaders — new outputs added under `HAS_VISIBLE_DIFFUSE`**
- `visibleDiffuseSeedF.glsl`, `alphaF.glsl`, `pbralphaF.glsl`, `fullbrightF.glsl`,
  `waterF.glsl` (class1 AND class3), `underWaterF.glsl`.
- **AUDIT: for EVERY one — does adding `layout(location = N)` declarations change attachment-0
  output in any permutation? Any shader that now declares outputs it does not always write? Any
  permutation combination where the new code path is compiled but the attachments do not exist?**
- **AUDIT: `fullbrightF.glsl` now publishes ZERO diffuse for all legacy fullbright. Confirm this
  cannot affect `frag_color`.**

**E. Shader program setup**
- `add_common_permutations` unchanged; water/underwater permuted separately via a new
  `sl_visible_diffuse_sidecar()` helper, AFTER each program's `clearPermutations()`.
- Seed program decoupled from the `success` chain, no `llassert`.
- **AUDIT: any program that now gets `HAS_VISIBLE_DIFFUSE` but draws to a target WITHOUT the extra
  attachments — including HUD, impostor, probe, snapshot and shadow passes.**

**F. Bridge publication**
- Two-stage seeded/resolved latches for visible diffuse AND surface coverage.
- Generation-tracking array grew to 9 with a `static_assert`; loop bound derived not hardcoded.
- `reserved[0]` renamed `reset_event_counter` (same offsets/size).
- Settings listener for `RenderVisibleDiffuseSidecar` via `handleSetShaderChanged`.
- **AUDIT: the listener triggers a shader reload AND buffer realloc. Can toggling the setting at a
  bad moment (mid-frame, during snapshot, while a probe renders) crash or leave a target unbound?**

**G. Unrelated-to-sidecar**
- M6: deleted `sHasLastVelocityProjection = false` on cube/probe passes.
- Particle suppression in `LLViewerPartSim::updateSimulation()` — `setDead()` on SPIRAL/BEAM/CHAT
  sources while `RENDER_DEBUG_FEATURE_UI` is off, behind `BDMergeHideEffectParticlesWithUI`.
- **AUDIT M6 specifically: it changed already-shipped velocity/TAA behaviour. Is deleting that
  `else` definitely right for EVERY consumer of the reset flag, not just the sidecar?**

## What I want

1. **Any further instance of the class that caused the regression** — a sidecar/bridge change that
   alters state the beauty pass, or any other existing feature, depends on. This is the priority.
2. Anything that only works because of an accident of current pool order, current settings, or
   current shader permutations, and would break if any of those changed.
3. Whether the **feature-OFF path** is genuinely inert. `RenderVisibleDiffuseSidecar` defaults off,
   and users who never enable it must get byte-identical stock rendering. Prove or disprove it.
4. The in-world checks that would have caught tonight's regression BEFORE the user saw it, phrased so
   they can be run in one login.

Report MUST-FIX / SHOULD-FIX / NOTE with file, line, and a concrete failure scenario. If the body of
work is sound apart from what is already known, say so plainly — do not manufacture findings. But
weight the search toward the beauty pass and the feature-off path, because that is exactly where the
per-change reviews were not looking.
