# BDMerge — Multi-Light / Projector Accumulation Brief

**Repo:** `I:\alchemy-machinima` (deferred renderer). Target HW: RTX 5090 32GB / 9950X / 192GB.
**Scope:** Read-only investigation. Do multiple projector / point lights on the same surface accumulate additively and correctly, or does a cap drop some?

> Line numbers cite `I:\alchemy-machinima` as of this writing. `pipeline.cpp` is under concurrent edit (projector-volumetrics work), so anchors may drift by a few lines — the quoted code/markers are the reliable anchors.

---

## Headline answer

**Yes — multiple projectors and point lights on one surface accumulate additively and correctly, in HDR, and the result reaches the tonemapper.** There is **no averaging, no overwrite, and no total-brightness clamp.** Each light is drawn as its own screen-space pass with GL blend `GL_ONE, GL_ONE` into an `RGBA16F` float buffer, so overlapping lights simply sum and get brighter until the tonemapper rolls the highlights off.

The only "cap" that matters is a **per-frame count of local lights = `RenderLocalLightCount` (default 256)**, and lights beyond it are dropped **by nearest-to-camera distance** (closest kept). That default is already very generous; nothing needs raising for typical machinima. The old hard cap of 6 (`MAX_LOCAL_LIGHTS`) applies **only to the legacy forward/non-deferred renderer**, which machinima does not use.

**Projectors have no extra *lighting* limit** — they obey the same 256 count. They do have a separate, smaller **shadow-casting** cap (`BDMergeMaxSpotShadows`, default 2, clamp `[2,6]`). A projector past the shadow-slot limit **still lights the surface, just without a cast shadow** — it is not dropped from lighting.

---

## 1. Local-light accumulation — additive and correct

### Blend state (the key fact)
`LLPipeline::renderDeferredLighting()` sets additive blending before the local-light passes:

- `pipeline.cpp:11137` — `gGL.setSceneBlendType(LLRender::BT_ADD);` (guarding the whole local-light block).
- `BT_ADD` maps to `blendFunc(BF_ONE, BF_ONE)` → `glBlendFunc(GL_ONE, GL_ONE)` (`indra/llrender/llrender.cpp`, `case BT_ADD`). True additive: `dst += src`.
- `LLGLEnable blend(GL_BLEND)` is on for the block (top of `renderDeferredLighting`).

### Destination is HDR float (no precision/clamp loss on the sum)
- `pipeline.cpp:983` — `mRT->screen.allocate(resX, resY, GL_RGBA16F)`. The accumulation target is a half-float HDR buffer, so summed lights are stored unclamped (well past 1.0).

### The per-light passes that add into it
The local-light block iterates `mNearbyLights` and dispatches every qualifying light as its own draw (`pipeline.cpp` ~11151 `"renderDeferredLighting - local lights"`):
- **Point lights, camera outside volume:** one additive cube pass with `gDeferredLightProgram` (→ `pointLightF.glsl`).
- **Point lights, camera inside volume:** batched into `fullscreen_lights` / `light_colors` and drawn in groups by `gDeferredMultiLightProgram` (→ `multiPointLightF.glsl`), up to `LL_DEFERRED_MULTI_LIGHT_COUNT = 16` per batch (`llviewershadermgr.h:33`). Batching is a dispatch optimization only — inside the shader each light is summed in a loop (`multiPointLightF.glsl:102` / `:133`, `final_color += …`), and each batch is itself additively blended, so totals are identical to one-pass-per-light.
- **Spotlights/projectors, camera outside:** pushed to `spot_lights`, drawn additively with `gDeferredSpotLightProgram` (→ `spotLightF.glsl`).
- **Spotlights/projectors, camera inside:** pushed to `fullscreen_spot_lights`, drawn with `gDeferredMultiSpotLightProgram` (→ `spotLightF.glsl` with `MULTI_SPOTLIGHT`).

Every one of these is a separate additive contribution. In screen space "per surface" = per pixel: for a given pixel, **every light whose bounding volume covers it and that survives the count cap contributes one additive term.** No per-pixel light limit exists in the deferred path.

Each shader writes linear HDR and only floors at zero — no upper clamp on the emitted value:
- `pointLightF.glsl:155` — `frag_color.rgb = max(final_color * final_scale, vec3(0));`
- `multiPointLightF.glsl:176` — `frag_color.rgb = max(final_color * final_scale, vec3(0));`
- `spotLightF.glsl` (enve line ~275; alchemy equivalent) — `frag_color.rgb = final_color * final_scale;`

### Reaches the tonemapper
After the local-light block, `mRT->screen` (with the summed HDR lighting) flows into `generateLuminance(&mRT->screen, …)` then `tonemap(&mRT->screen, …)` in the post pipeline (`pipeline.cpp`, post/DoF path). So the additive HDR result is exactly what the tonemapper rolls off — more overlapping lights → brighter overlap → tonemapper compresses. **Correct.**

**Verdict: additive-and-correct. No defect.**

---

## 2. The nearby-lights / local-light cap

### The effective cap: `RenderLocalLightCount`, default **256**
- `settings.xml` → `RenderLocalLightCount` "Number of local lights to render."
- Read in both `calcNearbyLights` and `setupHWLights` and the render loop:
  - `pipeline.cpp:6472` / `:6737` — `static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);`
  - `pipeline.cpp:11127` — same, in `renderDeferredLighting`.
- The render loop truncates here:
  - `pipeline.cpp:11170` — `if (count > local_light_count) { break; }` — stops emitting once 256 lights have been drawn this frame.

### What builds `mNearbyLights`, and what gets dropped
`LLPipeline::calcNearbyLights` (`pipeline.cpp:6462`) builds `mNearbyLights`, a `light_set_t` **sorted closest-first by distance to camera**. In the **deferred** path it inserts *all* in-range lights with no truncation:
- `pipeline.cpp` (calcNearbyLights): new lights within `max_dist` are inserted; the `MAX_LOCAL_LIGHTS`-based erase only runs `if (!LLPipeline::sRenderDeferred …)`. Insertion guard is `if (LLPipeline::sRenderDeferred || mNearbyLights.size() < MAX_LOCAL_LIGHTS)`.
- `max_dist` in deferred = `RenderFarClip` (the whole draw distance), so distance culling is only the normal far-clip / per-light radius test, not an artificial squeeze.

So the set can hold hundreds of lights; the **render loop's 256 count** is the real ceiling. Because the set is distance-sorted, if a scene ever exceeds 256 simultaneous local lights, the **256 closest to the camera are kept and the farthest are dropped** — a sensible criterion (far lights contribute least). Dropped lights also fade via the `light->fade` mechanism rather than popping.

### The `MAX_LOCAL_LIGHTS = 6` red herring
- `pipeline.cpp:6478` — `const S32 MAX_LOCAL_LIGHTS = 6;`
- This is used **only** in `!sRenderDeferred` (legacy forward) branches of `calcNearbyLights`. In deferred rendering (what machinima uses) it never truncates. Do not confuse it with the deferred cap.

---

## 3. Projector-specific behavior — lighting cap vs shadow cap

### Lighting: no extra limit
Projectors are gathered inside the same local-light loop (same `count > local_light_count` guard, `pipeline.cpp:11170`) and split into `spot_lights` / `fullscreen_spot_lights`. Those two lists are then drained **in full** — every projector that passed the 256 count draws its additive pass. There is **no separate, smaller projector lighting cap.**

### Shadows: separate, smaller cap (`BDMergeMaxSpotShadows`)
This fork already generalized the classic 2-slot spot-shadow limit into a configurable one:
- `pipeline.h:757` — `static constexpr U32 MAX_SPOT_SHADOWS = 6;`
- `pipeline.h:759/845–847` — `mSpotShadow[MAX_SPOT_SHADOWS]`, `mShadowSpotLight[MAX_SPOT_SHADOWS]`, `mSpotLightFade[…]`, `mTargetShadowSpotLight[…]`.
- `pipeline.cpp:469` — `static U32 bdmergeMaxSpotShadows()` → `LLCachedControl<U32> max_spots(gSavedSettings, "BDMergeMaxSpotShadows", 2)` → `llclamp(max_spots, 2u, MAX_SPOT_SHADOWS)`. **Default 2, range 2–6.**
- Shadow-map allocation and render loops honor it: `pipeline.cpp:1190` (`num_spots`), `:1232`, `:9629`, `:10662` (`4 + MAX_SPOT_SHADOWS` shadow matrices, marked `[BDMerge NSpot]`), `:13490` (cull-result array sized `MAX_SPOT_SHADOWS` — fixes a prior 2-element overflow crash).

### Non-shadow projectors still light — confirmed
Shadow slots are assigned by priority (`getSpotLightPriority()`), so only the top-N projectors get a shadow map. Projectors without a slot pass `proj_shadow_idx = -1` to the shader. In `spotLightF.glsl`:
- `if (proj_shadow_idx >= 0) { … shadow = texture(lightMap,…) … }` — the shadow lookup is **skipped** when the index is `-1`, leaving `shadow = 1.0` (fully lit).
- The projected diffuse/ambient/spec terms then accumulate normally (`final_color += …`).

So a projector past the shadow cap **illuminates the surface exactly as a shadowless projector** — it is never removed from the lighting pass. The shadow cap and the lighting cap are fully independent.

---

## 4. Clamping that could limit brightening

No clamp caps the accumulated total. The only clamps are localized and do not stop overlap from getting brighter:

- **Per-light PBR reflectance clamp `[0,10]`** on the *BRDF response only*, not the light energy:
  - `pointLightF.glsl:117`, `multiPointLightF.glsl:123`, `spotLightF.glsl:180/186` — `final_color += intensity * clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10));`
  - `intensity` = `dist_atten * color * 3.25` is **outside** the clamp, so brighter light colors still scale linearly. The clamp only bounds the normalized `nl*(diffuse+specular)` reflectance term (guards against fireflies), and 10× headroom is far above physical reflectance.
- **Legacy spot specular highlight clamp `[0,1]`** (spotLightF legacy branch, `speccol = clamp(speccol, vec3(0), vec3(1))`) — bounds a single specular highlight term only; the diffuse/ambient projector contribution is unclamped and each light still adds.
- **Floors at zero** (`max(final_color, 0)`) — prevents negative subtraction, does not cap the top.
- **No LDR clamp before tonemap:** destination is `RGBA16F` (`pipeline.cpp:983`); the additive sum stays HDR all the way into `tonemap()`.

`final_scale = 0.9` when `classic_mode > 0` is a uniform ~10% dim of each light under the classic/auto-adjust sky, applied equally to all lights — it does not break additivity or introduce a ceiling.

**Verdict: nothing clamps the multi-light total. Overlap brightens correctly and the tonemapper does the roll-off.**

---

## 5. Recommendation

**No change required for accumulation.** Multiple projectors/point lights on one surface already sum additively in HDR, non-shadow projectors still light, and there is no brightness ceiling before the tonemapper. The system behaves as machinima wants.

**If a scene ever needs more than 256 simultaneous local lights** (very unlikely for a single set), the single lever is trivial and cheap on a 5090:

- **Lever:** raise `RenderLocalLightCount` (default 256) via Debug Settings — no rebuild needed. It is already a live `LLCachedControl`. The only cost is more additive fullscreen/cube passes per frame (fill-rate bound); on a 5090 at typical machinima resolutions this is comfortably affordable to several hundred. No VRAM cost (these are screen-space passes, not per-light buffers).
- **More projector shadows:** raise `BDMergeMaxSpotShadows` (default 2 → up to 6). Each extra slot allocates one spot shadow-map render target and one extra shadow render pass per frame, so this is the more expensive knob — but 6 is fine on target HW. Beyond 6 requires bumping `MAX_SPOT_SHADOWS` (`pipeline.h:757`) and rebuilding; the arrays and cull-result sizing already key off that constant, so it is a clean single-constant change if ever wanted. Note `bdmergeMaxSpotShadows()` reallocates shadow targets on change via the shadow-resize path.

No gating flag is needed for the count bump — it is already user-facing and defaults high.

---

## Anchor index (alchemy-machinima)
| What | Site |
|---|---|
| HDR accumulation buffer | `pipeline.cpp:983` (`mRT->screen … GL_RGBA16F`) |
| Additive blend for local lights | `pipeline.cpp:11137` (`BT_ADD`); `llrender.cpp` `case BT_ADD` → `BF_ONE,BF_ONE` |
| Local-light gather | `calcNearbyLights` `pipeline.cpp:6462` |
| Deferred = insert all in-range (no truncate) | `calcNearbyLights` deferred branch (`sRenderDeferred` guards the erase) |
| `RenderLocalLightCount` (256) | `pipeline.cpp:6472/6737/11127`; `settings.xml` |
| Render-loop cap / drop | `pipeline.cpp:11170` (`count > local_light_count → break`) |
| Forward-only 6-cap | `pipeline.cpp:6478` (`MAX_LOCAL_LIGHTS`, `!sRenderDeferred` only) |
| Local-light dispatch block | `pipeline.cpp` ~11151 |
| Projector passes | `spot_lights` / `fullscreen_spot_lights` drained in full (~11274 projectors) |
| Multi-light batch size 16 | `llviewershadermgr.h:33` (`LL_DEFERRED_MULTI_LIGHT_COUNT`) |
| Spot-shadow cap (2, [2,6]) | `pipeline.cpp:469` (`bdmergeMaxSpotShadows`), `pipeline.h:757` (`MAX_SPOT_SHADOWS=6`) |
| Non-shadow projector still lights | `spotLightF.glsl` `if (proj_shadow_idx >= 0)` else `shadow=1.0` |
| Per-light BRDF clamp [0,10] | `pointLightF.glsl:117`, `multiPointLightF.glsl:123`, `spotLightF.glsl:180/186` |
| Shader HDR output (no top clamp) | `pointLightF.glsl:155`, `multiPointLightF.glsl:176`, `spotLightF.glsl` final |
| Tonemap consumes HDR screen | `generateLuminance`/`tonemap(&mRT->screen,…)` in post path |
