# Relight-quality local lights (an in-engine interpretation) — design brief

**Date:** 2026-10-20 (rev 2, after adversarial review round 1)
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Review:** round 1 (independent Opus) found 4 must-fix, 7 should-fix and 4 nits. All were applied in
rev 2, and Claude re-verified the load-bearing ones (PBR alpha paths, light size = radius × 1.5). The rev-2
edits themselves aren't re-reviewed; the code-review loop must attack §3.2–§3.3 and §4.1 first. The
disposition is in the Appendix.
**Process (per `CLAUDE.md`):** Codex implements (`--write`, `--prompt-file`), an independent Opus
sub-agent reviews ADVERSARIALLY, loop to 0 must-fix, Claude builds once, the user tests in-world.
Codex never builds.

> **BASE BRANCH.** `file:line` references are from `origin/fix/animesh-clone-pose-polish` at `057e558`.
> Re-anchor against any unpushed local work before implementing.

---

## 0. Reference policy — READ FIRST

The quality target is **iMMERSE Ultimate: ReLight** (`MartysMods_RELIGHT.fx` and
`MartysMods_ReLightAddon.addon64`, by Pascal Gilcher).

- Those files are **proprietary**. The `.fx` header says: "Unauthorized copying of this file, via any
  medium is strictly prohibited. Proprietary and confidential." The `.addon64` is a compiled binary.
- **Do not commit them to this repo, do not read them, and do not port, paraphrase or transcribe their
  code.**
- Relight is used here as a **visual reference only**. The user runs it in ReShade alongside the viewer
  (they already do, through the ReShade bridge) and compares the look (§9).
- Everything in this brief comes from public techniques, cited by name: Karis 2013 (*Real Shading in
  Unreal Engine 4*), Lagarde & de Rousiers 2014 (*Moving Frostbite to PBR*), and screen-space shadow
  ray-marching as in the repo's own `doc/CINEMATIC_SSGI_DESIGN.md` §5.

**What Relight's look consists of** (observed behaviour and the shader's public UI and structure, not
its code):

1. Lights have a physical **size** (sphere radius), so highlights are broad and soft, and light wraps
   around forms.
2. **Physically based falloff** (inverse-square-like), with sane behaviour very close to or inside the
   light.
3. **Every light casts soft shadows** that are sharp where an object touches a surface and soften with
   distance (contact hardening). They come from rays traced through screen depth toward the light's
   area.
4. Fine **micro-shadowing** from surface detail.
5. It scales to many lights by sampling them stochastically and accumulating over time.

---

## 1. The gap today (what the code PROVES)

| Fact | Where |
|---|---|
| **Point lights have no occlusion at all**: distance × N·L added straight in, so they leak through walls. | `class3/deferred/multiPointLightF.glsl:109-125` (PBR loop), `:146-167` (legacy loop); `class3/deferred/pointLightF.glsl` |
| Falloff is the legacy window `2·(1 − (d/R + f)/(1 + f))²`, not inverse-square. Lights have **no size**. | `class1/deferred/deferredUtil.glsl:360-369` |
| Only projectors cast shadows (shadow maps). Shadowed-projector slots: `BDMergeMaxSpotShadows`, default 2, max 10. Rig projectors outrank world ones. | `pipeline.h:1335`; `pipeline.cpp:24391-24402` |
| A gated PCSS-lite soft-shadow path exists for projectors and the sun (Lighting 2.0). | `class1/deferred/shadowUtil.glsl:93-95`, `:145+` |
| Per-light extra shader data already flows to the local-light shaders for **Rig Rim**. **This is the plumbing precedent.** | see the next three rows |
| — Deferred: batched (`rig_rim_lights[]`) and per-volume point and spot (`rig_rim_light`). | `multiPointLightF.glsl:40`; `pointLightF.glsl:73`; `spotLightF.glsl:99`; filled at `pipeline.cpp:23513-23621`; per-volume spot data in `setupSpotLight` `pipeline.cpp:24474` |
| — Forward legacy: `calcPointLightOrSpotLight(..., vec4 rim)` (×3.25 constant). | `class2/deferred/alphaF.glsl:110, 373`; `class3/deferred/materialF.glsl:142, 539` |
| — **Forward PBR:** `pbrCalcPointLightOrSpotLightRimKeep` (×3.0 constant), used by PBR alpha and the Actor FX PBR pass. | `class1/deferred/deferredUtil.glsl:930`; `class2/deferred/pbralphaF.glsl:87, 163, 332`; `class2/deferred/sharedActorFxPbrF.glsl:108, 596` |
| Forward/alpha lights: 8 hardware units (unit 0 = sun, 7 local), filled by `setupHWLights()` with `paramsForVolume(light)`, so each slot knows its `LLVOVolume`. Upload is in `LLRender::syncLightState`. They're set up **after** `calcNearbyLights()` re-runs in `renderGeomPostDeferred`, i.e. after deferred lighting. | `llrender.h:64`; `pipeline.cpp:10680`, `:7617-7618`; `llrender.cpp:903-972` |
| The shader's light size is **radius × 1.5** (so R ≤ 30 m in shader units). Batching applies only to lights whose box contains the camera; every other light is a per-volume box draw. | `pipeline.cpp:23436`; `llviewershadermgr.cpp:2714` |
| Batched fullscreen point lights go 16 per pass (`LL_DEFERRED_MULTI_LIGHT_COUNT`). | `llviewershadermgr.h:36`; `pipeline.cpp:23580-23602` |
| The deferred light loop counts lights **before** filtering (culled lights still use budget; stock shared code, off-limits). | `pipeline.cpp:23427-23460` |
| Primitive clamps: light intensity [0, 1], radius ≤ 20 m. These are a **design input**, not bugs. | `indra/llprimitive/*`; `doc/CINE_LIGHT_RIG_STATUS.md` |
| The light rig's fixtures already carry a physical source size, `mSourceSizeM` (0.01–4 m). | `alcinelightrigmodel.h` (`LightBase::mSourceSizeM`, `FIXTURE_SOURCE_SIZE_MIN/MAX`) |
| A deterministic design for local-light contact shadows exists but is **not built**: Feature B, with its injection sites, march, determinism rules and spatial denoise. | `doc/CINEMATIC_SSGI_DESIGN.md` §5–§7 |

**What the viewer has that a ReShade effect doesn't:**

- real normals and materials from the G-buffer;
- the true 3D position, colour and radius of every light;
- shadow maps that see **off-screen** occluders.

The interpretation uses all three.

---

## 2. Scope and phasing (stated up front)

This is a **render-state feature** (new passes, a new render target, shader changes in every local-light
path). `CLAUDE.md`'s "ship whole" rule explicitly allows slicing for exactly this, with an in-world A/B
between steps. Three phases, each complete and usable on its own, each **default OFF and byte-identical
when off**:

| Phase | Delivers | Risk |
|---|---|---|
| **1 — Light shape** | Sphere-light shading (soft highlights, wrap) + optional physical falloff, in every local-light path including alpha/forward | Low: shading-only, no new passes or targets |
| **2 — Soft area shadows** | A visibility pre-pass for the top-K lights, soft area-light shadows with contact hardening, spatial denoise, plus projector hybrid | Medium–high: new pass + target, touches lighting composition |
| **3 — Quality and scale** | Depth pyramid (Hi-Z) for long rays, per-tile light selection instead of global top-K, optional micro-shadows | Medium |

**Out of scope:** stochastic many-light sampling with temporal reuse (ReSTIR) and temporal denoising.
They conflict with the machinima determinism rule (§6), and SL scenes rarely have more than a handful of
lights that matter. Indirect bounce is out too (SSGI Feature A covers it separately).

---

## 3. Phase 1 — light shape and falloff

### 3.1 Per-light data

Add one `vec4 light_shape` per light, carried exactly beside the Rig Rim data in every path (§1 row 5):

| Field | Meaning |
|---|---|
| `x` | source radius in metres (0 = point light, exact legacy path) |
| `y` | falloff mode: 0 = legacy window, 1 = physical |
| `z` | calibration gain for physical mode (§3.3); 1 when mode 0 |
| `w` | Phase 2 shadow channel index, −1 = none. **Always −1 in forward/alpha paths** (§4.1) |

**Plumbing sites** (mirror Rig Rim exactly):

- the deferred fills (`pipeline.cpp:23513-23621`) and `setupSpotLight` (`:24474`);
- for forward: a new `LLLightState` field plus a setter that bumps `mLightHash`, its upload in
  `syncLightState` (`llrender.cpp:903-972`), its snapshot/restore copy, and zeroing wherever the rim
  data is zeroed (`pipeline.cpp:10737, 10841, 10853, 10864`);
- the new reserved uniform appended **in lockstep** in the `llshadermgr.h` enum and the `push_back`
  list (assert at `llshadermgr.cpp:2146`);
- when the feature is off, **skip the uploads**, as Rig Rim does (`pipeline.cpp:23517, 23594, 23622`).

**Where the radius comes from:**

- **Rig emitters:** `mSourceSizeM / 2` from the fixture in fixture mode. For legacy (non-fixture) rig
  lights, use `RenderLocalLightDefaultRadius`. Look it up by emitter id through the manager, as
  `ALCineLightRigManager::rigRimParamsFor(id, out)` does.
- **Rig bounce omnis:** the same **source** radius as their projector (they model the same fixture's
  diffuse bounce).
- **World lights:** SL has no size field. Use the global `RenderLocalLightDefaultRadius` (default 0 =
  unchanged).

### 3.2 Shading (public techniques)

Apply only when `x > 0`; when `x == 0` the existing code runs unchanged.

- **Split the light vector.** `pbrPunctual` (`deferredUtil.glsl:726`) uses one light vector for both
  diffuse and specular. The `x > 0` branch needs two: the **centre** direction (with a horizon factor)
  for diffuse, and the **representative point** for specular.
- **Specular (PBR paths):** the representative-point method (Karis 2013). Pick the closest point on the
  sphere to the reflection ray. **Keep the original α in D** and only multiply the specular result by the
  normalisation `(α / α')²`, where `α' = saturate(α + r / (2·d))`. Doing both would darken twice.
  - The `r/(2d)` constant matches UE4's shipped code as recalled; the reviewer couldn't fetch the
    notes. **Codex: verify it against Karis 2013 before using it.**
- **Specular (legacy non-PBR loops):** these use the `lightFunc` glossiness LUT, so α-normalisation
  doesn't apply. v1 gives legacy materials **diffuse wrap only**, plus the representative-point
  direction for the highlight position, and no energy normalisation.
- **Diffuse:** sphere-light irradiance with horizon handling (Lagarde & de Rousiers 2014, "sphere or
  disk" illuminance). This gives the wrap that makes a big soft source read as big.
  - That illuminance **already contains the distance term** (sin²σ). So:
    - **Mode 0 (legacy falloff):** use the **ratio** `E_sphere / (π·sin²σ)` in place of `nl`, and keep
      the legacy `dist_atten`. Otherwise distance falloff is applied twice.
    - **Mode 1:** use `E_sphere` directly with the §3.3 window and gain.
- **Early-outs:** the existing `nl <= 0` early-outs (the `multiPointLightF.glsl` legacy loop,
  `pointLightF.glsl:142`, `spotLightF.glsl:195, 242`) must move inside the `x == 0` branch. Otherwise
  the wrap never shows.
- **Inside the sphere** (`d < r`): clamp to the full-hemisphere case, never NaN or a spike.

### 3.3 Physical falloff (mode 1)

`E = (1 / max(d², max(r, r_floor)²)) · saturate(1 − (d/R)⁴)²` — windowed inverse-square (Karis 2013).

- `R` is the **shader** light size, radius × 1.5 (`pipeline.cpp:23436`; up to 30 m), the same `R` the
  legacy window uses.
- `r_floor` = `RenderLocalLightNearFloor` (default 0.1 m).
- **Without a floor this explodes:** with r = 0, `1/d²` is unbounded. Worked example (reviewer
  arithmetic): an SL radius-10 light with falloff 0.5 gets gain ≈ 14, and at 5 cm it would be about
  6,400× its legacy peak. Face lights sit at exactly that distance.
- **Hard cap as well:** `gain·E ≤ k · legacyAtten(0)`, with `k = RenderLocalLightPhysicalMaxBoost`
  (default 4). The existing `clamp(…, 0, 10)` and `clampHDRRange` don't bound this.

**Calibration**, so switching modes doesn't change exposure:

- `z = legacyAtten(d_ref) / physicalAtten(d_ref)`, computed on the CPU per light.
- `d_ref` is the rig's effective orbit radius for rig emitters, and `R/2` for world lights.
- The gain is applied **in the shader**, not baked into the light colour, so the primitive's [0, 1]
  intensity clamp is untouched.
- The legacy window is algebraically the same in the deferred and forward paths (the ×3.25 and ×3.0
  constants cancel in the ratio), so one CPU gain works everywhere (reviewer-verified).
- Result: identical brightness at `d_ref`, brighter closer, darker farther — the physical look.

### 3.4 Injection sites

- Batched points, both loops in `multiPointLightF.glsl`;
- per-volume `pointLightF.glsl` and `spotLightF.glsl` (also used by the fullscreen spot program);
- forward legacy `calcPointLightOrSpotLight` in `class2/deferred/alphaF.glsl` and
  `class3/deferred/materialF.glsl`;
- **forward PBR** `pbrCalcPointLightOrSpotLightRimKeep` in `class1/deferred/deferredUtil.glsl:930`, used
  by `class2/deferred/pbralphaF.glsl` and `class2/deferred/sharedActorFxPbrF.glsl` (and its local copy in
  `pbralphaF.glsl:163`).

Alpha (legacy and PBR) is included in Phase 1, so hair and sheer cloth aren't lit with a different model
from the body.

**Settings:**

- `RenderLightShapeEnable` (bool, default false) — master;
- `RenderLocalLightPhysicalFalloff` (bool, default false);
- `RenderLocalLightDefaultRadius` (F32 m, default 0, range 0–1);
- `RenderLocalLightNearFloor` (F32 m, default 0.1);
- `RenderLocalLightPhysicalMaxBoost` (F32, default 4).

---

## 4. Phase 2 — soft area shadows

### 4.1 Architecture: a light-visibility pre-pass

Shadows can't be denoised after lights are summed, so they're computed **per selected light**, before the
lighting pass, into a visibility target. This is the "shadow mask" pattern.

1. **Light selection (CPU, deterministic).** Each frame, pick the top **K** local lights (default 4, max
   8):
   - **Lights with no shadow map first** (point lights and non-projector spots, the ones that leak),
     then by estimated screen contribution: projected radius × luminance × falloff to the camera.
     Don't rank rig emitters first: rig projectors already win the shadow-map slots
     (`pipeline.cpp:24391-24402`), and a rig would fill all K channels, so the leaking world light would
     never get one.
   - **Input:** rank from `mLights` through a pure filter (on, in range of the view frustum), then
     intersect with what the deferred pass will actually draw. **Not** from `mNearbyLights`: at
     pre-pass time it's last frame's set, its membership fades out on wall-clock time, its distances are
     stale, and ties fall back to drawable pointer order (`pipeline.cpp:7617`, `:10639-10651`;
     `pipeline.h:1845-1852`).
   - Ties are broken by light id, so the ranking is a pure function of scene and camera.
   - Each selected light gets a channel `0..K−1`, written into `light_shape.w` in the **deferred**
     paths only.
   - Log the cap whenever more candidates exist than K (never a silent truncation).
2. **Visibility pass** (fullscreen, half resolution by default). For each selected light and each pixel,
   march `S` rays (default 4) from the surface toward sample points on the light's **disk**:
   - the disk faces the pixel, with radius = the light's source radius;
   - use the SSGI design's march (§5.2: SSR helpers, thickness test, off-screen ⇒ lit) — linear in Phase
     2, Hi-Z in Phase 3;
   - visibility = the fraction of unblocked rays. The light's size produces contact hardening naturally:
     an occluder near the receiver blocks all samples, while one far away blocks only some.
   - Output: an RGBA8 target (K ≤ 4) or two (K ≤ 8).
3. **Denoise:** an edge-aware bilateral blur with depth and normal weights, plus a bilateral upsample to
   full resolution (SSGI design §7). **Spatial only.**
4. **Lighting (deferred only):** in the deferred local-light shaders, if `light_shape.w ≥ 0`, fetch
   that light's visibility channel (`texelFetch` or `textureLod`, not an implicit-LOD sample inside the
   loop) and multiply its contribution by it. Unselected lights are unchanged (or, optionally, take SSGI
   Feature B's single hard contact ray).
   - **Forward/alpha paths never apply the visibility channel** (`light_shape.w` is forced to −1). The
     pass marches from the **opaque** depth, so an alpha fragment (hair in front of a shadowed wall)
     would read the visibility of the surface behind it. Forward lights are also set up after the
     pre-pass from a different list (§1). Alpha soft shadows would need their own per-fragment march;
     that's deferred (D6).

### 4.2 Projectors (hybrid)

A selected projector uses `min(shadow map, screen visibility)`. The shadow map supplies off-screen
occluders; the screen trace adds contact detail. Its existing PCSS-lite softness stays as it is.

### 4.3 Where it does NOT run (v1)

Use one explicit predicate, `areaShadowsAllowedThisView()`. It's false for:

- reflection-probe captures **and hero mirrors**, which both render through `updateProbeFace` and set
  `gCubeSnapshot`. Local lights do run in captures when the probe level is > 0 (`pipeline.cpp:23345`), so
  this matters;
- Prism/VCam feeds (`sPrismLensRender`, `llprismlens.cpp:7586`; **not** covered by `gCubeSnapshot`);
- `sRenderingHUDs`, `sImpostorRender` and `sReflectionRender` (water reflection).

In those views nothing is allocated or run and `light_shape.w = −1`, so they render exactly as today.
Extending to VCam feeds is a later decision.

**Snapshots:**

- **Tiled high-res snapshots:** the march treats occluders in neighbouring tiles as off-screen, so
  they're lit, and the blur can't see across tile edges. Expect seams (inference; `rawSnapshot` isn't
  traced). v1: disable area shadows for tiled snapshots, with a log line, or document the seams.
- **Resolution:** denoise radius and jitter are in pixels, so scale them by resolution relative to a
  1080p reference, so a high-res capture isn't noisier than the preview.
- **Allocation:** reallocate the half-res target alongside `mRT`, including under
  `RenderResolutionDivisor` (`pipeline.cpp:1141`).

**Settings:**

- `RenderAreaShadows` (bool, default false) — master;
- `RenderAreaShadowLights` (K, default 4);
- `RenderAreaShadowSamples` (S, default 4);
- `RenderAreaShadowSteps` (default 12);
- `RenderAreaShadowMaxDist` (m);
- `RenderAreaShadowThickness` (m);
- `RenderAreaShadowHalfRes` (default true);
- `RenderAreaShadowDenoise` (radius).

Quality presets (Low / Medium / High / Cinematic) set S, steps and resolution together.

---

## 5. Phase 3 — quality and scale

- **Depth pyramid (Hi-Z, min-depth mips)** so long rays stay accurate and cheap. Built only when
  `RenderAreaShadows` is on.
- **Per-tile selection** (for example 16×16 tiles, K per tile) instead of global top-K, for scenes with
  many lights. Ranking stays deterministic.
- **Optional micro-shadows:** a very short screen-space ray along the light direction to catch
  surface-detail occlusion. Default off.

---

## 6. Determinism — the hard rule (from `CINEMATIC_SSGI_DESIGN.md` §6)

Machinima frames must be reproducible: re-rendering frame N from the same recorded camera gives
**bit-identical** output.

- Sample the **current frame only**. No reprojection, no history buffers.
- Seed every random offset (disk samples, march jitter) from a hash of integer `gl_FragCoord.xy` plus
  a hash of the **light id** — not the channel, or the noise jumps when channels swap. **Never** from a
  frame counter, time, or `noiseSine`.
- No temporal accumulation. Optional preview-only accumulation is **deferred** (open decision D3). If
  ever added, it must switch off whenever Capture mode is armed.
- **Validation harness:** render the same frame twice with the feature on, **at a fixed resolution**,
  and diff. Must be identical. Log `[RelightQ] determinism PASS/FAIL`.

The consequence, stated honestly: at equal cost this will be somewhat noisier or softer than Relight,
which accumulates over time. More samples close the gap at GPU cost.

---

## 7. Render-state rules (`CLAUDE.md` rule 1)

Each phase's review must explicitly check:

1. **Feature OFF is inert.** No new target is allocated, no pass runs, and no new uniform is uploaded.
   A runtime-uniform branch **can't prove byte identity** (added code can change how the compiler
   schedules the untouched path). So either:
   - use a `#define` shader permutation — and add it **after** `clearPermutations()` (the known ordering
     hazard that once erased a permutation); or
   - prove it with an **off-vs-baseline image diff** on a fixed test scene.
   Either way, the image diff is required evidence.
2. **The beauty pass is unchanged** in blend state, colour mask and draw buffers. The visibility pass
   saves and restores every piece of GL state it touches, and the lighting passes' blend setup is
   untouched.
3. **Probe, mirror and VCam views** are unaffected (§4.3).
4. **The ReShade bridge** is unaffected (no G-buffer layout change). Note: iMMERSE RTGI reads the
   G-buffer, and running Relight on top of an in-engine-shadowed scene double-lights it. That's expected
   during A/B (§9), not a defect.

---

## 8. Instrumentation and tests

**Log verdicts** (`[RelightQ]`), each PASS / FAIL / INCONCLUSIVE:

- per-second summary: lights selected / candidates (K cap hit?), samples, resolution, pass GPU time;
- the determinism harness (§6);
- a calibration check: at `d_ref`, mode 1 brightness equals mode 0 within 1% for each rig light.

**TUT, pure math header `alrelightmath.h`** (literals, discriminating):

- The falloff calibration is identity at `d_ref`, monotonic, and zero at `d ≥ R`.
- The representative-point normalisation never increases energy (integrate over a hemisphere grid).
  The `x == 0` branch is **exactly** the legacy result, and `r → 0` converges to it **within a
  tolerance** (it's a limit, not an identity).
- **Near-field bound:** with r = 0 and d → 0, `gain·E` never exceeds `k · legacyAtten(0)`, and is finite.
- **Mode-0 sphere ratio:** at large d (small σ), `E_sphere / (π·sin²σ)` → `nl`, so wrap vanishes for small
  or distant lights.
- Inside-sphere is finite.
- Disk sampling is deterministic for a given (pixel, channel), uniform over the disk, with zero samples
  outside the radius.
- Light ranking is a pure function (permuting the input order gives the same selection), with ties
  broken by id.

---

## 9. In-world A/B against Relight (the reference) — outcomes stated in advance

**Setup:** a set with a wall, a floor, and an avatar standing 0.5 m from the wall. Use one rig key light
(fixture mode, source size 0.6 m) plus one world point light behind the wall.

1. **Phase 1:**
   - The highlight on skin broadens and softens as source size rises; at 0.01 m it matches today.
   - Physical falloff on: the subject's brightness is unchanged, the wall behind gets darker, and close
     objects get brighter.
   - Alpha hair is lit consistently with the body.
2. **Phase 2:**
   - The point light behind the wall **no longer leaks** onto the avatar.
   - The avatar's shadow on the wall is sharp near the feet and softens with height (contact
     hardening).
   - Rendering the same frame twice gives `determinism PASS`.
3. **Against Relight:**
   - Turn the in-engine features off, and place Relight lights in its add-on at roughly the same spots.
     Capture.
   - Then turn Relight off and the in-engine features on. Capture.
   - **Expected:** comparable softness and contact hardening on on-screen occluders. The in-engine
     version is **better** on off-screen occluders (shadow maps) and on materials (real roughness and
     normals). Expect it to be somewhat noisier at equal sample counts (§6).

---

## 10. Off-limits

- The Relight files: never read, port, transcribe or commit them (§0).
- G-buffer layout and formats; the ReShade bridge ABI.
- `indra/llprimitive/*` light clamps (a design input).
- The count-before-filter ordering in `renderDeferredLighting` (stock shared code).
- Shadow-map allocation and the `BDMergeMaxSpotShadows` policy.
- Never build. Claude builds after review convergence.

---

## 11. Open decisions (defaults chosen)

- **D1 — World-light default radius:** 0 (world lights unchanged unless the user sets it).
- **D2 — K:** 4 shadowed lights, unshadowed (leaking) lights first.
- **D3 — Preview-only temporal accumulation:** deferred, to protect determinism.
- **D4 — VCam/Prism feeds:** excluded in v1.
- **D5 — Phase order:** 1 → in-world A/B → 2 → A/B → 3.
- **D6 — Alpha soft shadows:** deferred (a per-fragment march in the forward paths). In v1, alpha gets
  Phase 1 shading but no area shadows.

---

## Appendix — review round 1 disposition (applied in rev 2)

| # | Finding | Disposition |
|---|---|---|
| M1 | Injection list missed the forward PBR paths (`pbralphaF`, `sharedActorFxPbrF`, `deferredUtil` helper) | Added to §1 and §3.4 |
| M2 | Screen visibility can't apply to alpha fragments (marched from opaque depth; forward lights set up later) | Forward/alpha force `w = −1`; alpha soft shadows → D6 |
| M3 | Physical falloff unbounded near the light (about 6,400× at 5 cm) | Near floor `r_floor` plus hard cap `k · legacy(0)`; TUT added |
| M4 | Sphere irradiance × legacy falloff double-counts distance in mode 0; one light vector; `nl` early-outs kill wrap | Ratio in mode 0; split vectors; early-outs moved (§3.2) |
| S1 | Karis wording (multiply-only), constant unverified, legacy LUT path | Clarified; Codex to verify; legacy gets wrap only |
| S2 | Rig-first ranking defeats the leak fix | Unshadowed lights first |
| S3 | `mNearbyLights` stale and wall-clock-faded at pre-pass time; channel-seeded noise | Rank from `mLights`; seed by light id |
| S4 | Exclusion needs an explicit predicate (Prism, HUD, impostor, water reflection) | `areaShadowsAllowedThisView()` |
| S5 | Snapshot tiling seams, pixel-scaled denoise, allocation, harness resolution | §4.3 snapshot rules; fixed-res harness |
| S6 | Plumbing sites unlisted (`LLLightState`, `syncLightState`, zeroing, `setupSpotLight`, reserved-uniform lockstep, skip uploads when off) | §3.1 plumbing list |
| S7 | Runtime-branch byte identity unprovable | `#define` permutation or required image diff (§7) |
| Nits | Limit vs identity test; R is 1.5 × radius; bounce radius wording; explicit-LOD fetch | Applied |
