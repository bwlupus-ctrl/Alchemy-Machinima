# Cinematic Screen-Space GI + Local-Light Contact Shadows — Design Deep-Dive

Status: **DESIGN** (nothing built). Target branch: `feature/cine-light-rig`.
Scope: one screen-space ray-march system that delivers **two** products — dynamic near-field
diffuse **bounce (SSGI)** and **contact-shadow occlusion for local (non-projector) lights** —
both render-only and **scrub-safe** for machinima.

All `file:line` anchors below were verified against the current tree.

---

## 1. Why this, and why now

We already ship the *direct-light* and *far-field* halves of the lighting stack:

| Have | File / control | What it does | What it CANNOT do |
|---|---|---|---|
| Reflection-probe irradiance | `sampleReflectionProbes()` → `softenLightF.glsl:179` | Low-frequency **ambient diffuse** from baked cubemaps | Sharp, *dynamic*, near-field colored bounce; ignores your cine lights |
| Reflection-probe radiance + SSR | `glossenv` / `tapScreenSpaceReflection` | Glossy **specular** reflection | Diffuse bounce |
| SSAO | `deferredLight` lightmap, `RenderDeferredSSAO` | Darkens ambient in crevices | Adds *color*; it only subtracts |
| Local lights | `multiPointLightF.glsl:102` loop | Distance × N·L additive | **Any occlusion at all** — they leak through walls |

Two gaps remain, and they are the two things that read as "flat / last-gen":

1. **No dynamic diffuse bounce.** Probe irradiance is a slow, spatially-smeared average — it
   cannot throw a red couch's warmth onto a chin 20 cm away, and it does not respond to the
   Cinematic Lighting rig's dynamic fixtures at all.
2. **Local lights have no occlusion term.** Proven in `multiPointLightF.glsl:116-123`: the loop
   computes `dist_atten * lightColor` and adds it. There is no shadow sample, no visibility test.
   A wall between light and surface does not exist to that shader.

Both gaps are solved by the **same primitive**: a screen-space ray-march over the depth buffer.
- Point the march along the surface hemisphere and *gather radiance* → **SSGI bounce**.
- Point the march *at each local light* and *test for an occluder* → **contact shadow**.

Build the primitive once; wire it to two consumers.

---

## 2. Hard constraints (non-negotiable, machinima)

1. **Render-only.** No sim/state changes. Pure function of the G-buffer + settings each frame.
2. **Scrub-safe / deterministic.** Re-render of frame *N* from the same recorded camera MUST be
   byte-identical. This **forbids** the technique SSR uses — see §6. This is the single most
   important constraint and the reason we do not just copy SSR wholesale.
3. **Default-off ⇒ byte-identical.** With `RenderScreenSpaceGI=0` and `RenderLocalLightContactShadows=0`
   the rendered image is bit-for-bit what it is today. Every new code path guards on its setting.
4. **Compose, don't replace.** Must layer cleanly on top of probe irradiance (energy conservation,
   §5.3), the local-light loop, persona/cue lighting, tonemap.
5. **Linear HDR.** The whole deferred composite is linear HDR; tonemap is a *later* separate pass
   (`renderFinalize` → `colorCorrect()`, `pipeline.cpp:15852`). We operate before it.

---

## 3. Architecture overview

Two independent features share one GLSL ray-march include. They can ship in either order; contact
shadows are cheaper and land first (§10, Phase 1).

```
renderDeferredLighting()  [pipeline.cpp:16490]
  ├─ sun/shadow + SSAO lightmap                         (16545-16574)
  ├─ blur lightmap                                       (16576-16643)
  ├─ bind + clear screen_target (mRT->screen, RGBA16F)   (16645-16683)
  ├─ softenLight  ── sun + atmos + PROBE IRRADIANCE ─►   (16718-16751)   writes screen_target
  │
  │   ┌───────────────────────────────────────────────────────────────┐
  │   │  NEW PASS ①  gSSGIGatherProgram  (fullscreen)                  │  ← insert AFTER 16751,
  │   │    reads: screen_target (lit color = radiance src), g-buffer   │    BEFORE 16756
  │   │    writes: mSSGIMap (scratch RT)                               │
  │   │  NEW PASS ②  gSSGICombineProgram (fullscreen)                  │
  │   │    reads: mSSGIMap (+ denoise), carves probe AO, adds bounce   │
  │   │    writes: screen_target                                       │
  │   └───────────────────────────────────────────────────────────────┘
  │
  ├─ local lights:                                                        ← contact shadow (Feature B)
  │    gDeferredLightProgram        (per-volume points)   (16776-16912)   injected INSIDE these
  │    gDeferredSpotLightProgram    (per-volume spots)    (16914-16953)   shaders' add loops
  │    gDeferredMultiLightProgram[] (batched fullscreen)  (16955-17028)
  └─ setColorMask; forward geom (alpha/glow/water)        (17037-17110)
```

Two placement choices for the SSGI pass, and the tradeoff:

- **After soften, before local lights (recommended default).** Radiance source = sun + atmos +
  probe ambient. Clean, cheap, no feedback. Bounce does not include analytic-light energy.
- **After the local-light loop (before the color-mask reset at `pipeline.cpp:17037`).** Radiance
  source additionally includes your cine-rig local lights, so a spotlight's pool bounces onto the
  wall beside it — the more "cinematic" result — but the source buffer is heavier and you must be
  sure no light writes after you sample. Expose as a setting: `RenderScreenSpaceGISource` (0 =
  pre-local, 1 = post-local).

> **Never sample `mRT->screen` while it is bound as the render target.** Pass ① reads `screen` and
> writes `mSSGIMap`; Pass ② reads `mSSGIMap` and writes `screen`. This ping avoids read/write
> feedback (`pipeline.cpp` RT rules, confirmed by the plumbing map).

---

## 4. Feature A — SSGI diffuse bounce

### 4.1 The gather (Pass ①, `ssgiGatherF.glsl`)

Per output pixel `tc`:

1. Reconstruct view-space `pos = getPositionWithDepth(tc, getDepth(tc))`
   (`deferredUtil.glsl:588`, `:452`) and `n = getNorm(tc).xyz` (`:428`). Skip sky
   (`GET_GBUFFER_FLAG(... GBUFFER_FLAG_SKIP_ATMOS)`) — `gbufferUtil.glsl`.
2. Build a cosine-weighted hemisphere basis around `n`. Draw `RenderScreenSpaceGIRayCount`
   directions from a **spatial-only** blue-noise/Poisson set seeded purely from `gl_FragCoord`
   (see §6 — NOT from a frame counter).
3. For each ray, march the **current** depth buffer with the shared `traceScreenRay`-style loop
   (§4.2). On a hit at UV `h`:
   - Sample radiance `L = texture(sceneMap /*=screen_target*/, h).rgb`.
   - Reject back-faces: require `dot(getNorm(h).xyz, -rayDir) > 0` (hit surface faces us).
   - Weight by cosine term `max(dot(n, rayDir), 0)` and a distance falloff
     `1/(1 + d²·RenderScreenSpaceGIFalloff)`, clamped past `RenderScreenSpaceGIMaxDistance`.
4. Accumulate `bounce += L * weight`; track `hitWeight` for confidence. Output
   `mSSGIMap.rgb = bounce / max(sampleCount,1)`, `mSSGIMap.a = confidence` (fraction of rays that
   hit on-screen — drives the screen-edge fade and the probe-carve blend in Pass ②).

Half-resolution is the default (`RenderScreenSpaceGIHalfRes=1`): gather at ½×½, then bilateral
upsample in Pass ②. 4× fewer marches; the diffuse signal is low-frequency so it survives upsampling.

### 4.2 Reusing the SSR march

The core loop already exists: `traceScreenRay()` at `screenSpaceReflUtil.glsl:78`, with
`generateProjectedPosition()` (`:47`, view→UV via `projection_matrix`) and `getLinearDepth()`
(`:69`). We fork a trimmed copy into a shared include `screenSpaceRayUtil.glsl`:

- **Drop the previous-frame reprojection.** SSR transforms the ray by `inv_modelview_delta` to read
  *last frame's* `sceneMap`/`sceneDepth` (`screenSpaceReflUtil.glsl:80-84`). We sample the
  **current** frame, so we remove that transform entirely. (This is also mandatory for §6.)
- **Parametrize the direction.** SSR hard-codes `reflect(viewPos, n)`. Our gather passes an
  arbitrary hemisphere direction; the contact-shadow consumer passes `light.xyz - pos`.
- Keep the thickness test verbatim: `abs(delta) < distanceBias` is the hit
  (`screenSpaceReflUtil.glsl:116`), the adaptive/exponential step (`:130-146`), and the binary
  refine (`:148-181`). Reuse `depthRejectBias` and `adaptiveStepMultiplier` as-is.

### 4.3 Combine + energy conservation (Pass ②, `ssgiCombineF.glsl`)

Naïvely adding SSGI to the frame double-counts, because probe irradiance already bakes some of the
same neighborhood into `iblDiff` (`softenLightF.glsl:179` → `deferredUtil.glsl:852`). We reconcile
on the **diffuse** term only (probe radiance/specular is untouched — the two are cleanly split at
`deferredUtil.glsl:656-663`).

Recommended model — **GI as a colored AO-carve** (energy-reasonable, art-directable):

```
// probe ambient already in the frame; SSGI confidence 'w' in [0,1] from mSSGIMap.a
// carve the flat probe ambient down by how occluded we are, then add real colored bounce
lit          = texture(screen, tc).rgb;          // includes probe iblDiff
bounce       = bilateralUpsample(mSSGIMap, tc).rgb * albedo(tc);   // color the bounce by receiver albedo
occlusion    = w * RenderScreenSpaceGIIntensity; // 0..1
lit         -= probeAmbientEstimate(tc) * occlusion * RenderScreenSpaceGIAOCarve;
lit         += bounce * RenderScreenSpaceGIIntensity;
screen.rgb   = max(lit, 0);
```

- At `Intensity=0` this is identity (default-off guarantee).
- The AO-carve term prevents "GI makes everything brighter" — where SSGI is confident and dark, we
  remove the flat probe fill it is replacing; where SSGI found bright neighbors, we add their color.
- `albedo(tc)` multiply keeps bounce physically tied to the receiver (a white wall bounces more).
- `probeAmbientEstimate` can be the same `amblit_linear` the composite used, re-derived cheaply, or
  a stored channel; simplest first cut is to skip the carve (`AOCarve=0`) and ship pure additive
  with a conservative default `Intensity≈0.5`, then add the carve in Phase 3.

Screen-edge fade: multiply the whole SSGI contribution by a vignette on `mSSGIMap.a` and screen
border (SSR already computes an analogous vignette at `screenSpaceReflUtil.glsl:339-346` — reuse the
idea) so bounce ramps to zero as its on-screen support disappears, hiding the screen-space seam.

---

## 5. Feature B — local-light contact shadows

Independent of SSGI, cheaper, and the direct fix for "lights leak through walls." No new RT needed.

### 5.1 Injection sites

The occlusion multiplier goes right before each `final_color += …` in the local-light shaders:

- **Batched points (PBR):** `multiPointLightF.glsl:118-123` — wrap `intensity` by `vis`.
- **Batched points (legacy):** `multiPointLightF.glsl:146-167` — wrap `col`.
- **Per-volume points:** `class3/deferred/pointLightF.glsl` (same loop shape).
- **Spots (non-projector):** `class3/deferred/spotLightF.glsl`.
  (Projector spots already sample a shadow map — leave those; gate contact shadows to lights with
   no projector.)

### 5.2 The visibility march

```glsl
// pos = view-space surface point; Lpos = light[i].xyz (view space); already have both
float contactShadow(vec3 pos, vec3 Lpos) {
    vec3  toL   = Lpos - pos;
    float distL = length(toL);
    vec3  dir   = toL / distL;
    float t     = RenderLocalLightContactShadowBias;          // start offset off the surface
    float march = min(distL, RenderLocalLightContactShadowMaxDist);
    float dt    = march / float(RenderLocalLightContactShadowSteps);
    for (int s = 0; s < RenderLocalLightContactShadowSteps; ++s) {
        vec3 p   = pos + dir * t;
        vec2 uv  = generateProjectedPosition(p);              // reuse SSR helper
        if (any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1)))) break; // off-screen ⇒ lit
        float sceneZ = getLinearDepth(uv);                    // reuse SSR helper
        if (-p.z > sceneZ + RenderLocalLightContactShadowThickness) return 0.0; // occluder ⇒ shadow
        t += dt;
    }
    return 1.0;                                               // unobstructed
}
```

Multiply `intensity`/`col` by this before accumulation. 8–16 steps is plenty; early-out on the
off-screen break keeps cost bounded. Because batched fullscreen lights loop `LIGHT_COUNT` per pixel,
gate the *whole* march behind the master toggle and consider limiting to the nearest *K* lights
(`RenderLocalLightContactShadowMaxLights`) to bound worst-case cost — and `log()` that cap so it is
never a silent truncation.

**Limitation (honest):** screen-space ⇒ only on-screen occluders shadow. For the wall-leak case the
wall is almost always on-screen between camera and lit surface, so this lands the punch. Off-screen
occluders (a wall behind camera) still leak — that is what the projector-shadow promotion path
(separate design) is for. Contact shadows soften a per-light penumbra by jittering the start `t`
with the same spatial noise as §6.

---

## 6. Determinism / scrub-safety — the machinima crux

**Why we cannot copy SSR's temporal tricks.** SSR is *deliberately* temporally variant:
- it samples the **previous frame** (`inv_modelview_delta`, `screenSpaceReflUtil.glsl:80-84`), and
- it **advances a Poisson window every main-camera bind** (`mPoissonOffset`,
  `pipeline.cpp:18131-18136`, uniform `noiseSine`).

Both mean the same frame re-rendered yields *different* pixels — fine for realtime, fatal for
scrub-safe machinima (frame N must be reproducible).

**Our rules:**
1. **Sample the current frame only.** No `*_delta` reprojection (already dropped in §4.2).
2. **Seed all noise spatially, not temporally.** Derive the per-pixel rotation/sample offset from a
   hash of integer `gl_FragCoord.xy` (optionally combined with the *deterministic* recorded camera
   transform, which is itself a pure function of the frame), and **never** from a running counter,
   `noiseSine`, or wall-clock time. Identical camera + identical frame ⇒ identical noise ⇒ identical
   output on every re-scrub.
3. **No temporal accumulation / TAA history.** We denoise **spatially only** (§7). This costs image
   quality per-frame, but machinima can afford more rays than realtime, and determinism is worth it.

A validation harness (§9) renders the same frame twice and diffs — must be bit-identical.

---

## 7. Denoise without a time axis

Spatial-only signal is noisier, so:

- **Edge-aware bilateral upsample/blur** in Pass ② (and a small à-trous pass if needed): weight
  neighbor taps by depth similarity (`getDepth`) and normal similarity (`getNorm`) so bounce does
  not bleed across silhouettes. Diffuse GI is low-frequency, so a wide bilateral kernel cleans it
  well without smearing geometry.
- **Half-res gather + full-res bilateral upsample** (default). The bilateral upsample uses the
  full-res depth/normal to keep edges crisp while the radiance came from ¼ the rays.
- Ray count and denoise radius are the two main quality/perf knobs (§8, presets §8.1).

---

## 8. Settings (all `settings.xml`, SSR group ~`14743-14819`; C++ three-site pattern below)

SSGI:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `RenderScreenSpaceGI` | Bool | 0 | Master toggle (0 ⇒ byte-identical) |
| `RenderScreenSpaceGIIntensity` | F32 | 0.5 | Bounce strength (0 ⇒ identity) |
| `RenderScreenSpaceGIRayCount` | S32 | 8 | Hemisphere rays per pixel |
| `RenderScreenSpaceGIStepCount` | S32 | 24 | March steps per ray (reuses SSR iteration logic) |
| `RenderScreenSpaceGIRayStep` | F32 | 0.1 | Base step (mirror of `…ReflectionRayStep`) |
| `RenderScreenSpaceGIThickness` | F32 | 0.5 | Hit tolerance (`distanceBias` analogue) |
| `RenderScreenSpaceGIMaxDistance` | F32 | 6.0 | Gather radius (m) before falloff hard-stops |
| `RenderScreenSpaceGIFalloff` | F32 | 1.0 | Distance falloff shaping |
| `RenderScreenSpaceGIHalfRes` | Bool | 1 | Gather at half-res + bilateral upsample |
| `RenderScreenSpaceGIDenoiseRadius` | S32 | 2 | Bilateral kernel radius |
| `RenderScreenSpaceGIAOCarve` | F32 | 0.0 | Probe-ambient carve amount (Phase 3; 0 = pure additive) |
| `RenderScreenSpaceGISource` | S32 | 0 | 0 = pre-local-light radiance, 1 = post-local |

Contact shadows:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `RenderLocalLightContactShadows` | Bool | 0 | Master toggle |
| `RenderLocalLightContactShadowSteps` | S32 | 12 | March steps toward light |
| `RenderLocalLightContactShadowThickness` | F32 | 0.35 | Occluder thickness tolerance |
| `RenderLocalLightContactShadowBias` | F32 | 0.05 | Start offset off surface (self-shadow guard) |
| `RenderLocalLightContactShadowMaxDist` | F32 | 8.0 | Cap march length (m) |
| `RenderLocalLightContactShadowMaxLights` | S32 | 8 | Nearest-K cap for batched lights |

Each cached setting needs the standard three sites (mirror the `RenderScreenSpaceReflection*`
family):
1. static member declare — `pipeline.cpp:400-405`
2. `connectRefreshCachedSettingsSafe("…")` — `pipeline.cpp:864-869`
3. read in `refreshCachedSettings()` — `pipeline.cpp:1846-1851`
Then upload as uniforms next to the SSR block in `bindReflectionProbes()` — `pipeline.cpp:18122-18141`
(SSGI passes) / directly in the local-light bind for contact shadows.

### 8.1 Quality presets (for the graphics-advanced / cine-rig UI)

| Preset | GI rays / steps | HalfRes | Denoise | Contact steps | Use |
|---|---|---|---|---|---|
| **Off** | — | — | — | — | Ship default |
| **Contact only** | GI off | — | — | 12 | Kill wall-leak, near-zero bounce cost |
| **Balanced** | 8 / 24 | on | 2 | 12 | Real-time-ish preview |
| **Cinematic** | 16 / 48 | off | 3 | 24 | Render/scrub passes; determinism on |

---

## 9. Render targets & shader registration

**New RT:** `LLRenderTarget mSSGIMap;` (standalone, near `mSceneMap` at `pipeline.h:1077`). Allocate
in `allocateScreenBufferInternal()` beside `mSceneMap` (`pipeline.cpp:1306-1313`), `GL_RGBA16F`,
half-res when `RenderScreenSpaceGIHalfRes`, gated on `RenderScreenSpaceGI`; `.release()` alongside
`mSceneMap.release()` (`pipeline.cpp:1913`). (If bilateral needs a second buffer, add `mSSGIMap2`.)

**New programs** (declare `extern` in `llviewershadermgr.h` near `:250`; define + `mShaderList`
push near `llviewershadermgr.cpp:448`; unload near `:1255`). Register like `gDeferredSoftenProgram`
(`llviewershadermgr.cpp:2516-2549`):
- `gSSGIGatherProgram`, `gSSGICombineProgram`: fullscreen deferred post — vertex
  `deferred/softenLightV.glsl`; `mFeatures.isDeferred=true`, `hasFullGBuffer=true`,
  `hasReflectionProbes = mShaderLevel[SHADER_DEFERRED]>2` (routes probe/scene-map/SSR uniform setup
  via `bindReflectionProbes`, `pipeline.cpp:16348`); `mShaderLevel = mShaderLevel[SHADER_DEFERRED]`.
- Contact shadows need **no new program** — they edit existing local-light shaders
  (`multiPointLightF`, `pointLightF`, `spotLightF`) behind a `#ifdef`/uniform guard, plus the shared
  `screenSpaceRayUtil.glsl` include for `generateProjectedPosition`/`getLinearDepth`.

**Shared include:** `screenSpaceRayUtil.glsl` (trimmed, current-frame fork of
`screenSpaceReflUtil.glsl`) attached to the gather shader and the local-light shaders.

---

## 10. Phased implementation

**Phase 1 — Local-light contact shadows (ship first).** Smallest surface, biggest correctness win,
no new RT. Add the shared march include; inject `contactShadow()` into `multiPointLightF` (both
paths), `pointLightF`, `spotLightF`; add the 6 settings + three-site plumbing; nearest-K cap with a
`log()`. Verify default-off byte-identical; A/B the wall-leak.

**Phase 2 — SSGI gather + additive combine (AOCarve=0).** New `mSSGIMap`, `gSSGIGatherProgram`
(half-res, current-frame march, spatial noise), `gSSGICombineProgram` (bilateral upsample + additive
`Intensity`). Insert the two passes after `pipeline.cpp:16751`. Ship the determinism harness (§9→§6).

**Phase 3 — Energy conservation + denoise polish.** AO-carve model (§4.3), à-trous denoise, screen-
edge vignette, `RenderScreenSpaceGISource` post-local option.

**Phase 4 — UI + presets.** Wire the four presets into graphics-advanced and/or the Cinematic Light
Rig floater; per-fixture "contribute to GI" and "cast contact shadow" toggles tie into the rig model.

Each phase is independently testable and independently revertible via its master toggle.

---

## 11. Risks & limitations (state them, don't hide them)

- **Screen-space blindness.** Off-screen geometry contributes neither bounce nor occlusion; both fade
  at frame edges (vignette hides the seam). Probes remain the far-field fallback — SSGI *layers on
  top*, it does not replace them.
- **Disocclusion / thin geometry.** The thickness test (`Thickness`) is a fudge; too small ⇒ missed
  hits (light leak returns), too large ⇒ haloing. Expose it; default conservative.
- **Half-res bleed.** Mitigated by bilateral upsample keyed on full-res depth+normal.
- **Feedback.** Avoided by the ping (read `screen`, write `mSSGIMap`; then read `mSSGIMap`, write
  `screen`). One bounce only — no infinite energy.
- **Cost of batched contact shadows.** `LIGHT_COUNT × steps` per pixel worst case; the nearest-K cap
  and off-screen early-out bound it. Measure on a dense-light scene before raising defaults.
- **Determinism regressions.** Any future edit that reintroduces a frame counter, `noiseSine`, or
  previous-frame sampling breaks scrub-safety silently — the §9 twice-render diff guards against it;
  keep it in the test suite.

---

## 12. Open questions for the author

1. GI radiance source default — pre-local (cheaper, stable) or post-local (cine lights bounce)?
   Leaning pre-local as default with `…GISource=1` opt-in.
2. Do we want per-fixture GI contribution weighting in the Cinematic Light Rig model, or a single
   global intensity for v1?
3. Contact shadows for *projector* spots too (softer contact on top of their shadow map), or points
   /non-projector spots only for v1?
4. Is half-res acceptable for final render passes, or do we force full-res when a scrub/record is
   active (tie to the same flag the deterministic path uses)?
