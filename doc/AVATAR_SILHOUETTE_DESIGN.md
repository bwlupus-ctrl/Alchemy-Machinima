# Avatar Silhouette — design (per-avatar, inside "Avatar Light Response")

Status: DESIGN ONLY, revision **R3** (2026-10-06), resolving Codex design review R3 (NOT APPROVED, 4 P0). Earlier rounds
verified: ReShade rebuttal, deferred/forward k scheme, 25-writer inventory, fast-bind site (R2); Keep-Rim finite
difference, frame-local queue guarding, overlay-after-renderFinalize (R3). L4 and L7 are accepted as scoped limits.
Precedence: where an R-table and a section differ, the SECTION text is authoritative (R-tables are history). Codex implements
per WP via `--prompt-file`; a fresh Codex reviews each WP; then the repo-mandated adversarial cross-package review;
then ONE build (CLAUDE.md). Branch `fix/animesh-clone-pose-polish`, HEAD `7a6d4e5e5e3`; every `file:line` anchor was
read at that HEAD. Parent design: `doc/AVATAR_LIGHT_RESPONSE_DESIGN.md` (called **ALR**; its sections are `ALR §n`).

Ships whole: model + store + uniforms + every shader path + G-buffer carrier + post-deferred coverage mask +
forced-emissive fallback + UI + persistence + instrument.

## R2 changes (Codex R2 resolution)

| # | Finding (substance) | Resolution | § |
|---|---|---|---|
| P0-1 | Actor-FX PBR scaled `color` BEFORE `actorFxPbrPostLight` (sharedActorFxPbrF.glsl:582, :611). The treatment is nonlinear and makes colour from zero input (actorFxF.glsl:1486-1499, :1547-1555, :1580-1585, :1957), so Layer looks survived at k = 0 | sharedActorFxPbrF now evaluates the **normal styled result unscaled**: lit (incl. rim) → `actorFxPbrPostLight` → `alrGraphicCover` (z only; the R1 k-change to this helper is reverted). Then `k·styled` + `(r−k)·ΔR`, where `ΔR` is an **isolated post-treatment rim delta** = styled(lit) − styled(lit − Rim). The second post-light call happens only when Keep Rim is on, a look is active and rim light is present; with no look, `ΔR = Rim` exactly (actorFxF.glsl:1492-1495). Look and authored emission × k. The forward light helper now returns the rim through an `out` parameter so Rim is available unmixed. Every other look path is either pre-light (actorFxApply on albedo, scaled at the output) or actorghost (next row). `actorFxPbrPostLight` has exactly one caller (sharedActorFxPbrF.glsl:611; grep) | §6.5 |
| P0-1b | (self-found while fixing P0-1) Shared Actor FX replays through actorghost programs and system-body replays got identity from ALR (CLONE-only upload, llactormover.cpp:12159, :12783), so a resident with a legacy Cover/Layer look would not silhouette | New upload slot `ALR_SLOT_SIL_ONLY`: trims identity, silhouette live. Used for every non-CLONE ghost style and for `renderSystemActorGhost` (llactormover.cpp:15093). actorghostF applies `k·styled + fill` after ALL look processing, for any `ghostLook`, and `× k` on the look glow (:1533) | §5.3, §6.6 |
| P0-2 | Phase B ran at pipeline.cpp:22198 after fullscreen passes, while world matrices were only captured at :22045-22046 | **Matrix contract:** Phase B saves the gGL projection/model-view stacks, `gGLModelView`/`gGLProjection` and `gGLLastMatrix`. It loads the world matrices captured at :22045-22046 (`actor_path_projection` / `actor_path_modelview`, same idiom as `render_actor_path_guides` :22056-22082), sets the scene viewport and restores everything afterwards (`ScopedActorMaskState` + explicit matrix restore). Phase A runs inside renderGeomPostDeferred with the live camera and loads `gGLModelView` / `gGLProjection` like renderActorMask (:23814-23819). New tests T6b/T20 check hair coverage AFTER on-lens filters and bloom (:22177, :22185) | §4.4 |
| P0-3 | L6: ghost overlay/proxy clones exposed to haze/froxel/shafts | **L6 removed.** New ghost-renderer coverage hook `LLActorMover::renderGhostSilCover(phase)` (llactormover.cpp), called by Phase A and Phase B: (a) **deferred and forward proxy clones** replay `getGhostDeferredQueue(camera, GHOST_VIEW_WORLD_MAIN)` (pipeline.cpp:6278-6280) under `ScopedGhostTransform(proxy)` (:6343), same batch classification (`classifyGhostBatchPhase`, :6929): Phase A solid/masked batches, Phase B blend batches; (b) **shared Actor FX proxies** (`renderSharedActorStyleProxyItem`, :17955; `drawGeometryGhost` at :18035/:18054) replay through a new coverage mode of `drawGeometryGhost` (`GhostDrawParams::mSilCoverPhase`, llactormover.h:1015): `apply_program` (:11962) binds the coverage program, only SWEEP_SOLID (A) / SWEEP_BLEND (B) run (:12085), SWEEP_GLOW (:12955) and style sweeps are skipped, static faces (:12615) included; (c) **Studio overlay clones** (`renderStudioGhosts`, llviewerdisplay.cpp:1790) draw AFTER `renderFinalize` (:1747), after every fog and volumetric pass, so no pass can fog them: Haze OFF holds by construction | §4.4, §3.3 |
| P2 | Tests/WPs | WP2 (slot), WP4 (ghost hook, matrix contract), WP5 (actorghost, SIL_ONLY uploads, sharedActorFxPbrF) extended. T6/T11 updated; new T19 (Actor FX look regression at 0/50/100 %), T20 (post-bloom hair coverage); unit tests 18-19 | §10, §12 |

## R3 changes (Codex R3 resolution)

Codex R3 verified as correct: the Keep-Rim finite difference, frame-local queue guarding, and that overlays draw after
renderFinalize.

| # | Finding (substance) | Resolution | § |
|---|---|---|---|
| P0-1 | `RimSplit` distributed `final_scale` (`nr·fs + rim·fs`), which is not bit-exact with today's `(nr + rim)·fs` (deferredUtil.glsl:930-987) | `RimSplit` returns RAW components and does not apply `final_scale`: `out vec3 nonrim` (after the debug-mode-2 zeroing), `out vec3 rim_raw`, `out bool rim_on`, `out float light_scale`. The old `RimKeep` recombines in today's order: `vec3 c = nonrim; if (rim_on) c += rim_raw; return c * light_scale;`. Silhouette callers branch: `alrSilOn() ? (nonrim·k + rim_raw·r)·light_scale : RimKeep(...)`, so the off path executes exactly today's expression. Parity is checked by a permutation-gated reference (T21) | §6.5 |
| P0-2 | `ALR_SLOT_SIL_ONLY` "uploads nothing" without a silhouette, so the previous draw's silhouette could leak | Every `upload_alr` call writes ALL FOUR uniforms (`alrEnabled`, `alrParams`, `alrSil`, `alrSilAux`) in every branch, including no-slot, identity and SIL_ONLY-without-silhouette (`alrEnabled = 0`, params `(0,1,1,1)`, sil/aux 0). The values come from a pure, unit-tested `ALLightResponse::uploadPlan(...)` (model), so the alternation silhouetted → plain → silhouetted is a CPU test. lldrawpool.cpp:472-489 is restructured around it | §5.3, §10.1 |
| P0-3 | Proxy-queue coverage replayed only `GhostBatch`, but `GhostProxy::mStaticFaces` is also harvested (llactormover.cpp:14511-14526; llactormover.h:912) | The proxy replay draws `proxy.mStaticFaces`: Phase A `mAlphaKind` 0 (solid) and 1 (cutoff, `mCutoff`), Phase B `mAlphaKind` 2 (blend), llactormover.h:882-883 | §4.4 |
| P0-4 | `ScopedActorMaskState` (pipeline.cpp:23419-23523) does not preserve blend factors/equations or stencil, and Phase B / `drawGeometryGhost` change blend factors (llactormover.cpp:12867-12955) | New RAII `ScopedSilCoverState` around both phases and the ghost hooks, nested inside `ScopedActorMaskState`. It saves the RGB/alpha blend factors (`GL_BLEND_SRC_RGB/DST_RGB/SRC_ALPHA/DST_ALPHA`), the blend equations (`GL_BLEND_EQUATION_RGB/ALPHA`), the `GL_BLEND` enable and the `GL_STENCIL_TEST` enable. It forces stencil off, and restores factors through `gGL.blendFunc` (4-factor form, llrender.h:476-477) via a GL→`eBlendFactor` reverse map, so LLRender's cache (llrender.h:602-605) and the indexed draw-buffer guard (llrender.h:480-494) stay coherent. Equations are restored raw. Depth enable/write/func, cull and polygon-offset enables stay RAII (`LLGLDepthTest`, `LLGLEnable/Disable`, as renderActorMask :23708-23711) | §4.4, §11.6 |
| P1 | `ScopedGhostTransform` (pipeline.cpp:6042) and `classifyGhostBatchPhase` (:6105) are pipeline-local | Proxy-clone replay is **pipeline-side**: `LLPipeline::renderGhostSilCover(phase)` in pipeline.cpp beside the ghost passes. It uses both helpers directly, with guards `gCubeSnapshot / sReflectionRender / sRenderingHUDs / sCurCameraID` (:7066-7072) and `mover.hasValidGhostDeferredQueueThisFrame(camera, GHOST_VIEW_WORLD_MAIN)` (:7078) before `getGhostDeferredQueue`. Only the shared-Actor-FX re-submission stays llactormover-side (`LLActorMover::renderSharedProxySilCover(phase)`, because `drawGeometryGhost`, :11823, is file-local there) | §4.4, §12 |
| P2 | Missing automated/state tests | Unit: `uploadPlan` alternation, RimSplit recombination-order mirror. In-world instrumented: T21 RimSplit bitwise parity (reference permutation), T22 Phase-B state invariant (blend/stencil/depth/cull/offset/FBO/draw buffers/viewport/matrices compared before/after, `[ALR] SILCOVER STATE-LEAK` verdict), T23 static proxy coverage and stale-queue rejection counters | §10, §12 |

## R1 changes (Codex R1 resolution)

| # | Finding (substance) | Resolution | § |
|---|---|---|---|
| P0-1 | Haze OFF broken on opaque fullbright (hazeF reads the G-buffer pixel behind) and on forward alpha (froxel/shafts see the background carrier) | Haze/water-haze/froxel/shaft passes no longer read the G-buffer carrier. They read a new **post-deferred silhouette coverage mask** (`RG8`, shared depth) built by a depth-tested replay of the silhouetted avatars' own geometry: Phase A (deferred + fullbright opaque/masked, before `doAtmospherics`) and Phase B (blended alpha, before the volumetrics in `renderFinalize`). This also fixes a self-found R0 bug: a world fullbright occluder in front of a silhouette was un-hazed by the carrier behind it. L3 deleted | §4.4, §6.3, §5.4 |
| P0-2 | Metallic PBR mid-fade went as k² (base × k fed F0, then spec × k again) | The G-buffer is no longer scaled. Base colour, spec, env, ORM stay authored; every deferred light pass scales its own **output** (non-rim × k, rim × r) from the per-pixel carrier; forward PBR scales the sun/IBL block output and uses a new light helper that applies k after the clamp. F0 and `(1−F)` always see the unscaled base: `mix(Lit, F, a)` is exact for every material | §2, §6.2-§6.6 |
| P1-1 | Frame mask missed `bindDeferredShaderFast` (pipeline.cpp:22933-22990) | One helper `uploadSilhouetteFrame(shader)` called in both `bindDeferredShader` (after :22999) and the fast branch (beside :22949) | §5.4 |
| P1-2 | RGBA format change flips ReShade's emissive semantic | **Refuted with code, plus guarded:** the viewer sets `SEM_VALID_EMISSIVE` from texture presence, not format (llreshadebridge.cpp:133-136, :247, :375); the add-on has no emissive slot, so `map_gl_internal_format` is never applied to it (sl_reshade_bridge.cpp:254-268 slot table, :1060-1072 pass-through). Only the descriptor's `gl_internal_format` value changes (0x881B→0x881A), not the ABI layout. In forced-emissive vintage mode the viewer does NOT publish the emissive texture, so the semantic stays exactly as today. New test T17 | §4.5, §5.5 |
| P1-3 | No-emissive fallback (black fill, no Haze/Rim) does not deliver the feature | **Forced silhouette-only emissive target:** when a silhouette is active and `RenderEnableEmissiveBuffer` is off, a non-persistent `AvatarSilhouetteForceEmissive` turns the attachment on through the existing rebuild handler, with an `EMISSIVE_SILHOUETTE_ONLY` define. Under that define every non-silhouette pixel keeps the vintage result (world emission written as 0, softenLight reads emission only for silhouette pixels). The feature is fully functional in vintage mode | §4.6 |
| P1-4 | Keep Rim not untouched: rim tint read the k-scaled diffuse | Follows from P0-2: the deferred passes compute the rim from unscaled G-buffer albedo and scale only its output; forward lights get the unscaled diffuse and a separate `rim_keep`. With Keep Rim ON the rim is bit-identical to the unsilhouetted rim at every amount | §6.3, §6.5 |
| P1-5 | Zero-cost claim overstated | Restated: no added GL calls, texture binds or fullscreen fetches; per fragment, one uniform `int` compare in adjusted-avatar programs and one in each fullscreen pass. Verified by T16 (Tracy GPU zones). A permutation gate was rejected (it doubles the avatar program set) | §9 |
| P1-6 | Target-alpha safety needs an exhaustive render-state contract | §4.3 lists all 25 attachment-3 writers (incl. diffuseAlphaMaskF.glsl:67), the clear value (1,0,1,1) → alpha 1.0 = invalid code, the colour mask, the blend state of the avatar G-buffer pools, and all three allocation sites (main, packs, impostor). Integer-exact fp16 encoding with a ±0.25 integrality test rejects blended or foreign values. Debug blend instrument; allocation-failure, copy and readback tests T18 | §4.2, §4.3, §10.2 |
| P2-1 | UI refresh used `getVisible() && !isMinimized()` | `isWidgetOnScreen(this)` (aluirefresh.h:510) plus a forced refresh in `onVisibilityChange(true)` | §7.3 |
| P2-2 | WP lists not exhaustive | §12 rewritten: 6 WPs, every file named, incl. llactormover, llreshadebridge, llviewercontrol, llviewershadermgr, the coverage shader | §12 |
| — | OPEN items | All RESOLVED per the user's standing instruction (take the recommendation) | §13 |

---

## 0. Summary

Four per-avatar controls in the Avatar Light Response floater turn any avatar (resident, self, animesh, Ghost Studio
clone) into a flat, unlit silhouette:

| Control | Symbol | Range / values | Default |
|---|---|---|---|
| Silhouette amount | a | 0–100 % (0 = today's lit avatar, bit-exact) | 0 |
| Fill colour | F | swatch + presets Black, Charcoal #262626, White, Chroma Green #00B140, Chroma Blue #0047BB, (Custom) | Black |
| Haze over silhouette | H | checkbox; ON = fog/haze/atmosphere lie over the fill, OFF = untouched by every participating medium | ON |
| Keep Rim | R | checkbox; ON = Rig Rim (and projector rim) edge still drawn, OFF = nothing lights the silhouette | OFF |

**Mechanism:** with `k = 1 − a`, every lit contribution is scaled by k **after** it is evaluated, and the rim by
`r = (Keep Rim ? 1 : k)`. `a·F` is added as unlit radiance. Deferred: output-side in each lighting pass, using a
per-pixel carrier in the emissive G-buffer alpha. Forward: in-shader. Result: `k·Lit + a·F = mix(Lit, F, a)`,
exact for every material at every amount (up to the shared amount quantisation, §4.2).

**Chroma guarantee (§3):** scene-referred. At 100 %, Haze OFF, Keep Rim OFF, every covered pixel holds exactly F in
the HDR scene buffer before exposure. The picked code reaches the screen exactly only when the post chain is the
identity on F. Exact codes under any tonemap/grade are delegated to Actor Chroma Key (RESOLVED-2).

---

## 1. User decisions (final, 2026-10-06)
1. Flat unlit fill in a user colour; picker + presets Black (default), Charcoal, White, Chroma Green, Chroma Blue.
2. "Haze over silhouette", default ON.
3. "Keep Rim": the Rig Rim on that avatar still draws its edge over the dark fill.
4. Amount 0–100 %, continuous (animatable) fade.
Per avatar, existing ALR target/session/inherit semantics; same LLSD with new keys; missing keys = off.
Open items resolved by the user's standing instruction: see §13.

---

## 2. Blend mathematics

Notation: `k = 1 − a_q`, `r = Keep Rim ? 1 : k`, where `a_q` is the quantised amount (§4.2) that the CPU uploads to
every program. `z, s, g` are the existing ALR Brightness, Tame keep and Glow (`alrParams.z/.y/.w`). All existing ALR
trims are evaluated first and are unchanged. Debug tint (`alrEnabled == 3`) overrides; the silhouette is inactive then.

### 2.1 Rule: scale outputs, never inputs
Scaling inputs (albedo, spec) is wrong for PBR metals (R1 P0-2): F0 = `mix(0.04, base, m)` (deferredUtil.glsl:1026-1032)
and the punctual `(1−F)` diffuse coupling (deferredUtil.glsl:726-781) would see the scaled base. So:
- **G-buffer:** albedo, spec, gloss, env, ORM, Tame carrier written exactly as today (ALR trims only). Only
  `frag_data[3]` changes: emission becomes `k·E + a·F` (PBR) or `a·F` (legacy), and its alpha carries the carrier.
- **Each deferred lighting pass** (softenLight, multiPoint, point, spot incl. actor-only programs) computes its
  result from the unscaled G-buffer exactly as today, then outputs `k·(non-rim) + r·(rim)`.
- **Forward lit shaders:** the sun/sky/IBL/emission block is evaluated unscaled and multiplied by k; local lights go
  through helpers that multiply the clamped light result by k and the rim by r; look/authored emission × k.
- **Nonlinear post-light treatments** (Actor FX `actorFxPbrPostLight`; actorghost looks) are evaluated on the unscaled
  lit result, and their OUTPUT is scaled by k. The kept rim is an isolated post-treatment delta (§6.5, §6.6).
- **Unlit (fullbright, fullbright-shiny, ghost overlay):** presented colour × k (after env mixing).
- **Glow passes:** final glow × k.
- **Fill:** `+ a·F`, once per pixel (§2.3, §2.4).

### 2.2 Exactness
Every term is `k × (today's value)`, so `Σ k·Tᵢ + r·Rim + a·F` = `k·Lit + a·F` (+ `(r−k)·Rim` by design when Keep Rim
is on). Exact because k multiplies after every non-linearity: the clamps `clamp(nl·(diff+spec),0,10)`, the classic-mode
sRGB recombine (deferredUtil.glsl:1083-1100), legacy gloss-env "energy conservation" `glossenv *= 1 − color`
(class3 reflectionProbeF.glsl:947), the legacy env mix (:954-963), and the PBR `(1−F)` coupling all happen inside
`Lit`. The only remaining approximation is the deferred haze-pass weighting mid-fade (§2.4); a_q quantisation is
shared by every term, so the pixel is the exact `mix(Lit, F, a_q)`.

### 2.3 Fill and final scale
softenLight multiplies its result by `final_scale = 1.1` in classic mode (softenLightF.glsl:310-314); the forward lit
shaders do the same after fog (pbralphaF.glsl:347-353, sharedActorFxPbrF.glsl:633-637, alphaF.glsl:392-400, class3
materialF.glsl:566-572).
- Deferred: softenLight writes `clampHDRRange(color·final_scale·k + emission_gbuffer)` for silhouette pixels, so the
  fill is F in both modes. In classic mode the `k·E` part of the emission misses the 1.1 mid-fade; accepted, ≤ 10 % of a
  fading term.
- Forward lit: the fill used is `F·w`, with `w = classic_mode > 0 ? 1/1.1 : 1` from the shader's own uniform.
- Unlit and ghost paths have no final scale and use F.

### 2.4 Haze algebra (fog is affine per pixel: `fog(x) = A·x + B`)
`atmosFragLighting` (atmosphericsF.glsl:35-42), water fog (waterFogF.glsl:99-105) and Cine Haze (cineHazeF.glsl:195-212)
are affine in colour.
- **Forward Haze ON:** add the fill before fog. `fog(k·L + a·F) = k·fog(L) + a·fog(F)` since `k + a = 1`. Exact.
- **Forward Haze OFF:** add `a·(F − fog(0))` after fog. Result `k·fog(L) + a·F`. Exact. Precedent for the `fog(0)` term:
  actorghostF.glsl:1522-1528.
- **Deferred passes** (hazeF / waterHazeF / froxelApplyF, all `dst' = S + dst·α` with `blendFunc(ONE, SRC_ALPHA)`:
  pipeline.cpp:25276, :25330, froxelApplyF.glsl:29,:82; projector shafts additive): with mask weight `h ∈ [0,1]`
  (§4.4), `src' = ((1−h)·S, 1 − (1−h)(1−α))`, so `dst' = (1−h)·(S + α·dst) + h·dst`; shafts `× (1−h)`. Exact at
  a = 0 and at a = 1 on full coverage; mid-fade and at partial-alpha edges the haze over the pixel fades with h.

---

## 3. Chroma accuracy — guarantee and limits

### 3.1 Decision: scene-referred fill
The fill is an unlit emitter of radiance `F = s2l(picked sRGB code)` placed in the HDR scene. Reasons:
- Haze ON, the fade, Keep Rim and alpha edge blending are scene-linear operations.
- A display-referred override needs a post-tonemap composite with alpha coverage. That is Actor Chroma Key's design
  (coverage pass + composite at the end of blitWithEffectsF, branch `feature/actor-chroma-key`).
- Inverting 9 tonemap types (tonemapUtilF.glsl:436-466), the shared grade (:470-481), COLOR_GRADE/LUT stages and the
  GPU-only auto-exposure is infeasible. Roto Ink's Scene layer made the same choice (pipeline.cpp:17187-17197).

### 3.2 Exact guarantee
At Amount 100 %, Haze OFF, Keep Rim OFF, on every pixel whose front-most surface is the avatar (deferred, fullbright,
forward alpha at full coverage, entity clones), the HDR scene value is **exactly F** (fp16 rounding) before exposure,
independent of sky, lights, probes, shadows, SSAO, classic mode, exposure state and every fog/volumetric pass.
On screen `display = Post(F)`. That equals the picked code (±1 code from colorCorrect dither) iff Post is the identity on
F:
- effective exposure 1 (RenderExposure 1 with dynamic exposure off, or the no-post legacy-gamma path);
- `tonemap_mix == 0` (`applyToneMap` returns the clamped linear value, tonemapUtilF.glsl:434, :481) or no-post;
- neutral grade, no on-lens filters, no bloom reaching the pixel, not an AA edge pixel.

Black is exact much more widely: 0 survives any exposure and every tonemap mapping 0→0 (T4 checks types 0-7). A grade
lift, contrast < 1 or a Uchimura pedestal raises it. S-Log (type 8) maps it to the log black code by design; with
type 8 auto-exposure is ignored (tonemapUtilF.glsl:426), so any fill is an exposure-stable log code.

### 3.3 Limits that remain (by design, not exceptions to Haze OFF)
- L1 Auto-exposure moves a scene emitter's code; keying requires dynamic exposure off. A large black silhouette lowers the meter.
- L2 A bright fill (White) blooms like any emitter. Authored and look glow are removed (§6.7).
- L4 Scene-wide layers drawn over the scene lie over the silhouette: Tron World / Roto Ink scene layers, weather lightning,
  Night Mask, on-lens filters, ReShade (incl. RTGI; with the visible-diffuse sidecar on, RTGI receives the k-scaled
  diffuse, §6.3; with it off, the raw G-buffer albedo).
- (L6 removed in R2.) Ghost clones: entity clones are real avatars (render maps); deferred/forward proxy clones and shared
  Actor FX proxies are replayed by the ghost coverage hook (§4.4). Studio overlay clones are drawn after `renderFinalize`
  (llviewerdisplay.cpp:1747, :1790), so no fog or volumetric pass reaches them and Haze OFF holds by construction. The
  stock overlay path is never fogged (its late compile has no fog branch, actorghostF.glsl:1538-1541), so on overlay clones
  Haze ON has nothing to lie over the fill. For fogged clones, use GhostDeferredEnable (proxy clones).
- L7 Partial-coverage edges and Phase B (§4.4): a non-silhouette translucent surface in front of a Haze-OFF silhouette
  is un-fogged by froxel/shafts in proportion to the silhouette coverage behind it (Phase B replays silhouette alpha only).

---

## 4. Per-pixel data

### 4.1 Emissive target becomes RGBA
`addDeferredAttachments` (pipeline.cpp:676-701): `U32 emissive = GL_RGBA16F;` (:680), non-HDR `GL_RGBA` (:689).
This is the only allocator of the G-buffer emissive attachment; it is used at all three sites: main (:1358), render-target
packs incl. aux/hero/probe/prism (:1751), avatar impostors (:29154).
- `GL_RGBA16F` and `GL_RGBA8` are required colour-renderable formats in GL 3.0+; `GL_RGB16F` is not required to be
  renderable (INFERENCE from the GL spec's required-format tables). So the change cannot add an allocation failure mode.
  An allocation failure takes today's path: `allocateScreenBuffer` returns false at :1358, and the pack path fails at :1751.
- Cost: `RGB16F` render targets are normally padded to 8 B/px (INFERENCE), so probably +0 B; worst case +2 B/px
  (+16.6 MB at 3840×2160). Measured in T16.
- ReShade: §4.5. Ghost-deferred diagnostics already handle `GL_RGBA16F` readbacks (llghostdeferreddiagnostics.cpp:760-766).

### 4.2 Carrier encoding (written by avatar G-buffer writers only)
The CPU quantises the amount once; every program (writers, forward shaders, light passes via the carrier) uses the same `a_q`.
```
HDR (RGBA16F, alr_sil_active bit4 = 1):  carrier = 2 + q + 512*flags      q in 1..510, a_q = q/510
                                          max 2 + 510 + 1536 = 2048 -> every value is an exact fp16 integer
LDR (RGBA8,   bit4 = 0):                  carrier = (q + 62*flags) / 255   q in 1..62,  a_q = q/62
flags: bit0 HAZE_SKIP (Haze OFF), bit1 RIM_CUT (Keep Rim OFF)
q = clamp(round(a*Q), 1, Q) for a > 1e-4 (Q = 510 or 62); a <= 1e-4 => silhouette off, nothing written.
Valid iff: frame mask != 0, pixel flag HAS_PBR or HAS_ATMOS (GET_GBUFFER_FLAG, with or without the avatar tag), and
  HDR: c = carrier - 2, |c - round(c)| < 0.25, 1 <= round(c) <= 2046, 1 <= (round(c) & 511) <= 510
  LDR: code = round(carrier*255), 1 <= code <= 248
```
Clear value 1.0 (§4.3): HDR c = −1 → invalid; LDR code 255 → invalid. The integrality test rejects blended values.
`AvatarLightResponse::sSilFrameMask` bit4 tells the shaders which encoding is in use. The CPU uses the same HDR predicate
as pipeline.cpp:683-684.

### 4.3 Render-state contract for attachment 3 (exhaustive)
Writers of `frag_data[3]` into the G-buffer FBO, all re-grepped at HEAD:
| Class | Files (line of the `frag_data[3]` write) | Alpha written | Flag (normal.w) |
|---|---|---|---|
| Avatar-capable writers (edited) | class3 materialF.glsl:604, class1 materialIndexedF.glsl:373, bumpF.glsl:134, diffuseF.glsl:114, diffuseIndexedF.glsl:117, diffuseAlphaMaskIndexedF.glsl:105, diffuseAlphaMaskNoColorF.glsl:101, avatarF.glsl:101, pbropaqueF.glsl:221, pbropaqueIndexedF.glsl:334 | 0 today; carrier when silhouette on | HAS_ATMOS / HAS_PBR (±avatar tag) |
| Geometry writers (not avatar programs) | diffuseAlphaMaskF.glsl:67, class1 materialF.glsl:48, pbrterrainF.glsl:436, terrainF.glsl:67, treeF.glsl:57, impostorF.glsl:62, highlightF.glsl:40, occlusionF.glsl:36 | 0 | HAS_ATMOS / HAS_PBR / SKIP |
| Sky-type writers | skyF.glsl:218, cloudsF.glsl:232, starsF.glsl:186, moonF.glsl:62, sunDiscF.glsl:53, meteorsF.glsl:64, auroraF.glsl:219 | non-zero | SKIP_ATMOS / HDRI → rejected by the flag gate; and < 2 → rejected by HDR encoding |
`ultimateDiopterGatherF` / `ultimateKaleidoF` declare `frag_data[3]` (3 outputs) into their own targets, not the G-buffer.

- **Clear:** colour mask (true,true) (llviewerdisplay.cpp:1022), clear colour (1,0,1,1) (:1032), clear (:1034); the
  profiling path clears with the current colour (pipeline.cpp:28876-28877). Unwritten pixels carry 1.0 (invalid).
- **Mask:** the G-buffer pool loop runs with `setColorMask(true,true)` (pipeline.cpp:7430) and resets to (true,false) at :7497.
- **Blend:** the avatar pool's G-buffer passes force blend off (`LLGLState blend(GL_BLEND, is_post_deferred_render ? …)`,
  lldrawpoolavatar.cpp:1116, :1201); the simple/bump pools disable it (lldrawpoolsimple.cpp:132, :257; lldrawpoolbump.cpp:978).
  Proof obligation 11.3 plus instrument: with `AvatarSilhouetteDebugCarrier` on, `upload_alr` checks
  `glIsEnabled(GL_BLEND)` when the bound draw FBO is `mRT->deferredScreen` and a silhouette is uploaded, and logs
  `[ALR] SIL-CARRIER-BLENDED program=<name>` once per program.
- **Impostors:** `upload_alr` uploads identity during `sImpostorRender` (lldrawpool.cpp:472), so impostor bakes never
  carry a carrier; silhouetted avatars never impostor (ALR §2.3).
- **Probe/mirror/prism packs:** same allocator and the same writers; their lighting passes read their own pack's carrier.

### 4.4 Post-deferred silhouette coverage mask (new; the source for every fog/volumetric pass)
- **Target:** `RenderTargetPack::silCover` (pipeline.h beside `actorMask` :1224), `GL_RG8`, sharing the pack's G-buffer
  depth. `ensureSilCover()` mirrors `ensureActorMask()` (pipeline.cpp:23369-23406): allocate on demand, share depth,
  FBO-complete check, never cache a failure. Released with the packs (as `releaseActorMasks`, :23351-23367). R = haze-skip
  weight, G = rim-cut weight.
- **Program:** `gSilCoverProgram[6]`, mirroring `gActorMaskProgram` (llviewershadermgr.cpp:5226-5262): same vertex shaders
  (`actorMaskV` / `actorMaskAvatarV` / `actorMaskEyesV`), same skinning/PBR permutations. New fragment
  `class1/deferred/silCoverF.glsl` reuses actorMaskF's authored-alpha lookup and cutoffs. It outputs
  `vec4(sil_cover.x·cov, sil_cover.y·cov, 0, cov)`: `cov = 1` (after the cutoff discard) for opaque/masked passes, and
  authored alpha × vertex alpha for blended passes. `sil_cover = (HAZE_SKIP ? a_q : 0, RIM_CUT ? a_q : 0)` is uploaded per draw.
- **Draw selection:** only draw infos whose owners resolve, via `ALAvatarLightResponse::peek(mActorFxOwner,
  mActorFxFallbackOwner)`, to a slot with a silhouette and at least one flag. System bodies go through a new
  `LLActorMover::renderSystemSilCover(av, cover, phase)` beside `renderSystemActorMask` (llactormover.cpp:15041).
- **Phase A** — inside `renderGeomPostDeferred`, immediately before `doAtmospherics()` (pipeline.cpp:7841-7845), when mask bit1
  or bit2 is set:
  - clear to 0;
  - draw the silhouetted avatars' opaque and masked geometry: every pass in `kActorMaskPasses` (pipeline.cpp:23527-23565)
    plus `PASS_FULLBRIGHT(_RIGGED)`, `PASS_FULLBRIGHT_SHINY(_RIGGED)`, `PASS_FULLBRIGHT_ALPHA_MASK(_RIGGED)`
    (lldrawpool.h:152-159, :206-207), visibility from `beginRenderMap(pass)` as in renderActorMask (:23734-23747),
    plus system bodies;
  - state as in renderActorMask (:23707-23715): no blend, depth test `LEQUAL` with no depth write, polygon offset from a
    new `AvatarSilhouetteCoverBias` (default −8), back-face cull; everything saved and restored by `ScopedActorMaskState`
    (:23419-23523).
  - Front-most-surface correctness: any occluder in front (deferred or fullbright, world or avatar) fails the depth test.
- **Phase B** — in `renderFinalize`, immediately before `renderFroxelVolumetrics` (pipeline.cpp:22198), main HDR view only,
  when bit1/bit2 is set and froxel or projector volumetrics will run this frame:
  - draw the silhouetted avatars' blended geometry: draw infos of `PASS_ALPHA` / `PASS_ALPHA_RIGGED` in
    `gPipeline.beginAlphaGroups()` / `beginRiggedAlphaGroups()` (same sources as lldrawpoolalpha.cpp:898-906), plus the
    system bodies' alpha layers;
  - depth `LEQUAL` with no write;
  - union blend `blendFunc(ONE, ONE_MINUS_SRC_COLOR)` on RG, so `w = 1 − Π(1 − hᵢ·covᵢ)`, saved and restored by the same
    scoped state. Phase A's contents are kept (no clear).
- **Ghost coverage hooks (R2 P0-3, R3 P1):** both phases also call two hooks, with the same program set, state and keys:
  - **Proxy clones** (GhostDeferredEnable) — **pipeline-side** `LLPipeline::renderGhostSilCover(phase)` in pipeline.cpp,
    beside `renderGhostDeferredOpaqueMasked` (:6268), because `ScopedGhostTransform` (:6042) and
    `classifyGhostBatchPhase` (:6105) are local to pipeline.cpp:
    - Guards, in this order:
      - return if `gCubeSnapshot || LLPipeline::sReflectionRender || LLPipeline::sRenderingHUDs ||
        LLViewerCamera::sCurCameraID != CAMERA_WORLD` (:7066-7072);
      - return if `!mover.hasValidGhostDeferredQueueThisFrame(*LLViewerCamera::getInstance(), GHOST_VIEW_WORLD_MAIN)`
        (:7078-7081; the silent probe, never the asserting accessor);
      - only then `getGhostDeferredQueue(...)` (:6278-6280). Each skip increments a counter (T23).
    - Skip `!proxy.mFrustumVisible` (:6338).
    - Per proxy, `ScopedGhostTransform transform(proxy)` (:6343) after loading the phase's world model-view.
    - Per `GhostBatch`, `classifyGhostBatchPhase(batch.mPass)` (:6929): SOLID/MASK in Phase A (drawn before `doAtmospherics`:
      G-buffer :6268-6350 and forward solids :24862-24864); BLEND in Phase B (`renderGhostRiggedBlend`, :6729, after
      `renderGeomPostDeferred`, :24878).
    - **Static faces (R3 P0-3):** per `GhostStaticFace` in `proxy.mStaticFaces` (llactormover.h:912; harvested at
      llactormover.cpp:14512, :14526), Phase A draws `mAlphaKind` 0 (solid) and 1 (cutoff `mCutoff`), Phase B draws
      `mAlphaKind` 2 (blend), llactormover.h:882-883. Same transform and keys; the static program variant.
    - Key chain `(proxy.mInstanceId, di->mActorFxOwner, di->mActorFxFallbackOwner)`, exactly as the proxy uploads
      (ALR §5.6).
  - **Shared Actor FX proxies** — llactormover-side `LLActorMover::renderSharedProxySilCover(phase)`, because
    `drawGeometryGhost` (:11823) is file-local to llactormover.cpp. For the proxy items consumed by
    `renderSharedActorStyleProxyItem` (:17955) whose owner resolves to a silhouette, it re-submits `drawGeometryGhost`
    (:18035, :18054) with `GhostDrawParams::mSilCoverPhase` (new field, llactormover.h:1015-1040).
    - In coverage mode, `apply_program` (:11962) binds `gSilCoverProgram[rigged ? RIGGED : STATIC]`, and the per-batch
      upload is `sil_cover` instead of the style uniforms.
    - `draw_batches` (:12085) runs SWEEP_SOLID only (A) or SWEEP_BLEND only (B); `draw_static` (:12615) is included;
      SWEEP_GLOW (:12955) and every style sweep are skipped.
    - Transforms and palettes use the function's own code. `renderSystemActorGhost` (:15093) system-body replays are
      covered by `renderSystemSilCover` on the native body.
    - These proxies draw at the avatar's live position (`mFootAgent = getRenderPosition()`, :16909) inside the
      post-water alpha stream (lldrawpoolalpha.cpp:843-845), so they belong to Phase B, plus their solid part in A.
  - **Studio overlays:** not replayed (§3.3).
- **Matrix and viewport contract (R2 P0-2):**
  - Phase A (inside renderGeomPostDeferred) uses the live camera: `gGL` model-view := `gGLModelView`, projection :=
    `gGLProjection`, `gGLLastMatrix = nullptr`, as renderActorMask does (:23814-23819).
  - Phase B (renderFinalize, after on-lens filters :22177 and bloom :22185 have run fullscreen passes):
    - save both gGL matrix stacks, the `gGLModelView`/`gGLProjection` globals, `gGLLastMatrix` and the viewport;
    - load the world matrices captured at :22045-22046 (`actor_path_projection`, `actor_path_modelview`) into the gGL
      stacks AND the globals (the ghost proxy paths load `gGLModelView` before their transforms, :6325-6327, and
      renderActorMask does the same, :23815),
      using the idiom of `render_actor_path_guides` (:22056-22082);
    - set the viewport to the scene render size (`mRT->deferredScreen` dimensions);
    - replay, then restore all of it; `ScopedActorMaskState` also restores FBO, draw buffers, masks, offset, cull,
      textures and the shader.
- **GL state contract (R3 P0-4):** `ScopedActorMaskState` (pipeline.cpp:23419-23523) saves colour mask, clear colour,
  polygon-offset factor/units, cull face mode, viewport, FBOs, read/draw buffers, scissor, matrices and textures, but not
  blend or stencil. Both phases and both ghost hooks therefore run inside a new nested RAII `ScopedSilCoverState`:
  - Save: `GL_BLEND` enable; blend factors `GL_BLEND_SRC_RGB`, `GL_BLEND_DST_RGB`, `GL_BLEND_SRC_ALPHA`,
    `GL_BLEND_DST_ALPHA`; equations `GL_BLEND_EQUATION_RGB` / `GL_BLEND_EQUATION_ALPHA`; `GL_STENCIL_TEST` enable.
  - Set: stencil off; blend per phase (A off; B on with `gGL.blendFunc(ONE, ONE_MINUS_SRC_COLOR, ONE, ONE_MINUS_SRC_COLOR)`;
    equation ADD).
  - Restore: factors through `gGL.blendFunc` (4-factor form, llrender.h:476-477) using a GL-enum → `LLRender::eBlendFactor`
    reverse map (llrender.h:356), so LLRender's cached factors (:602-605) and the indexed draw-buffer guard (:480-494; it
    re-asserts per-attachment state after every global change) stay coherent. Equations via `glBlendEquationSeparate`.
    Enables via `LLGLState`.
  - Depth (`LLGLDepthTest(GL_TRUE, GL_FALSE, GL_LEQUAL)`), `GL_CULL_FACE` and `GL_POLYGON_OFFSET_FILL` use the RAII
    objects renderActorMask uses (:23708-23711).
  - `drawGeometryGhost`'s own blend changes in coverage mode (it normally sets factors per sweep, llactormover.cpp:12867-12955)
    happen inside this scope and are undone by it. In coverage mode it must not reach the sweep blend calls at all (only
    SOLID/BLEND coverage sweeps run).
  - All changes go through gGL wrappers wherever LLRender has one, never raw indexed calls (llrender.h:480-494 contract).
  - Phase A runs inside the post-deferred pool loop, where the visible-diffuse guard may be armed
    (lldrawpoolalpha.cpp:849-861 explains it); `doAtmospherics` already makes global gGL mask/blend changes at the same
    point (pipeline.cpp:25275-25277).
- **Consumers:** hazeF, waterHazeF (Phase A content), froxelApplyF and the class3 projector-volumetric march (Phase A+B), via
  `uniform sampler2D silCoverMap` bound in `bindDeferredShader` when the mask is valid. Mask bit5 = "silCover valid this
  view"; the passes skip the fetch otherwise. The G-buffer carrier is used only by the lighting passes, which light exactly
  the G-buffer surface.
- **Cost:** zero when no silhouette has Haze OFF or Keep Rim OFF (no allocation, no pass, no fetch). When active: one
  replay of the silhouetted avatars' geometry with a trivial shader, plus Phase B for their alpha.
- **Template, not reuse:** `renderActorMask`, `drawActorMaskBatch` and Actor Mask state stay untouched. The new pass may
  call `ScopedActorMaskState` and read `kActorMaskPasses` (same TU, read-only).

### 4.5 ReShade bridge
- The viewer publishes `f.emissive` from attachment 3 when present (llreshadebridge.cpp:247) and sets
  `SEM_VALID_EMISSIVE` from `texture_present` (gl_name, width, height — :133-136, :375). The format is not part of validity.
- The add-on's replicated slots (sl_reshade_bridge.cpp:254-268) contain no emissive slot; EMISSIVE validity passes
  straight through (:1060-1072).
- So the RGB→RGBA change alters only the descriptor's `gl_internal_format` value. The `mLastTextures` comparison
  (llreshadebridge.cpp:333) fires a one-time change event. No ABI layout or version change.
- Forced-emissive vintage mode (§4.6): the viewer does not publish the attachment (`if (n > 3 && !silhouetteOnlyEmissive)`
  at :247), so EMISSIVE stays invalid exactly as in vintage today.
- The add-on is unchanged and stays OFF-LIMITS. T17 verifies.

### 4.6 Forced silhouette-only emissive target (vintage mode)
- New setting `AvatarSilhouetteForceEmissive` (Boolean 0, **Persist 0**), registered with the existing
  `handleEnableEmissiveChanged` listener (llviewercontrol.cpp:328-331, :1165). That handler runs
  `releaseGLBuffers / createGLBuffers` plus `setShaders` (:214-229, :318-326).
- Effective predicate `emissive = RenderEnableEmissiveBuffer || AvatarSilhouetteForceEmissive` at the three sites that read
  the setting: pipeline.cpp:682, llviewershadermgr.cpp:491 (`add_common_permutations`) and :1235 (utility `attribs`).
- When `!RenderEnableEmissiveBuffer && Force`, both shader-manager sites also define `EMISSIVE_SILHOUETTE_ONLY 1`.
- `ALAvatarLightResponse::tick()` (idle) sets `Force = TRUE` once when a silhouette becomes active and the emissive buffer is
  off. It stays sticky for the session: no repeated rebuilds, reset on relog. The floater shows "Preparing silhouette
  buffer…" for that frame. There is a one-time shader rebuild hitch.
- Under `EMISSIVE_SILHOUETTE_ONLY`, vintage output is preserved for every non-silhouette pixel:
  - pbropaqueF / pbropaqueIndexedF: the non-avatar line, and the avatar line when the silhouette is off, write `vec4(0)`;
    the avatar line writes `vec4(a·F, carrier)` (no authored emission, as in vintage);
  - pbrterrainF.glsl:436 writes `vec4(0)`;
  - softenLightF: at :157 `colorEmissive = alr_sil ? gb.emissive.rgb : vec3(0)`, and the `SKIP_ATMOS` branch (:226-236)
    takes its `#else` (baseColor) path;
  - every other reader of `gb.emissive.rgb` is a light pass that does not use it (multiPointLightF.glsl:97 and
    pointLightF.glsl:105 assign, never add).

---

## 5. Engine and C++ changes

### 5.1 Model — `alavatarlightresponsemodel.{h,cpp}` (pure, unit-tested)
Add to `Params` (h:37-44): `F32 mSilAmount = 0`, `LLVector3 mSilFillSrgb = 0`, `bool mSilHaze = true`,
`bool mSilKeepRim = false`.

New API:
```cpp
struct Packed { LLVector4 mParams; LLVector4 mSil; LLVector4 mSilAux; };
// mParams = today's pack(); mSil = (s2l(fill), a_q) or 0; mSilAux = (carrier, haze?1:0, keep_rim?1:0, 0) or 0
Packed    packAll(const Params& p, bool hdr_encoding);
S32       silQuant(F32 a, bool hdr);                 // q, 0 when a <= 1e-4
F32       silAmountQ(S32 q, bool hdr);               // a_q
F32       silCarrier(S32 q, bool haze_over, bool keep_rim, bool hdr);
bool      silDecode(F32 carrier, bool hdr, F32& a_q, bool& haze_skip, bool& rim_cut);   // CPU mirror of §4.2
F32 srgbToLinear(F32); LLVector3 srgbToLinear(const LLVector3&);
enum EFillPreset { FILL_BLACK, FILL_CHARCOAL, FILL_WHITE, FILL_CHROMA_GREEN, FILL_CHROMA_BLUE, FILL_CUSTOM, FILL_PRESET_COUNT };
LLVector3 fillPresetSrgb(EFillPreset); EFillPreset matchFillPreset(const LLVector3&);   // ±0.5/255
LLVector3 silForward(const LLVector3& lit, const LLVector3& fill_lin, F32 a_q, F32 fill_scale,
                     const LLVector3& fog_A, const LLVector3& fog_B, bool haze_over);   // §2.4, before final_scale
void      silHazeBlend(LLVector3& src_rgb, F32& src_a, F32 h);                          // §2.4 deferred weight
```
- Presets: #000000, #262626, #FFFFFF, #00B140, #0047BB. The last two match Actor Chroma Key's table
  (ACTOR_CHROMA_KEY_DESIGN.md:583-590 on that branch).
- `sanitize` (cpp:46-55) clamps amount and fill (non-finite → 0). `isIdentity` (:57-68) also requires
  amount ≤ 1e-4; bypass is still identity.
- `EPreset` gets `PRESET_SILHOUETTE` before `PRESET_COUNT` (value 8). It sets amount 1 and keeps everything else.
  RESET returns `Params()`.
- LLSD keys (cpp:157-178): `sil_amount` (Real), `sil_fill` (Array of 3 Reals, display sRGB), `sil_haze` (Boolean),
  `sil_keep_rim` (Boolean). Missing or ill-typed keys give the defaults (silhouette off). The file stays at version 1.

### 5.2 Store — `alavatarlightresponse.{h,cpp}`
- `Slot::mPacked` (h:56-61) becomes `ALLightResponse::Packed`. `lookup` / `peek` (h:104-105, cpp:423-434) return
  `const Packed*`; the semantics are unchanged.
- `ensureTable` (cpp:304-333) packs with the frame's HDR predicate and computes
  `static U32 sSilFrameMask`, zeroed at the master-off early return (:321-324):
  - bit0 any silhouette;
  - bit1 any Haze OFF;
  - bit2 any Keep Rim OFF;
  - bit3 debug carrier view (`AvatarSilhouetteDebugCarrier`);
  - bit4 HDR encoding;
  - bit5 is set per view by the pipeline when `silCover` is valid.
- Also in `ensureTable` (or `tick`): when bit0 is set and the emissive buffer is off, set `AvatarSilhouetteForceEmissive` (§4.6).
- `U64 revision()` is bumped in `markChanged` (:260), `clearAll`, `loadFromFile` and `importMerge` (UI rule 2).
- `debugLogTick` (:525-580) appends ` sil=<a_q> fill=#RRGGBB haze=<0|1> rim=<0|1>` per target and ` sil_mask=<bits>
  cover=<allocated|valid>` to the summary.
- Unchanged: persistence, scene I/O, copy/paste, capacity, impostor dirtying. A silhouetted avatar is non-identity, so it
  is kept live (ALR §2.3).

### 5.3 Upload — `lldrawpool.cpp` `upload_alr` (:460-490)
Upload `alrSil` and `alrSilAux` beside `alrParams`, always while `alrEnabled != 0` (a = 0 must reach a program that last saw
a silhouette). `ALR_SLOT_GLOW` (actorghost SWEEP_GLOW) uploads `z = g·k` and `alrSil = alrSilAux = 0` (no fill in the
additive glow sweep).

**Full-reset rule (R3 P0-2).** `upload_alr` (today :467-490) is restructured. Every call, in every branch, writes all
four uniforms: `alrEnabled`, `alrParams`, `alrSil`, `alrSilAux`. The values come from a pure model function:
```cpp
struct UploadPlan { S32 enabled; LLVector4 params, sil, aux; };
UploadPlan uploadPlan(const Packed* slot /*nullptr = absent/identity*/, EUploadSlot which, bool debug_tint);
```
- `nullptr` → `{0, (0,1,1,1), 0, 0}`.
- BEAUTY → today's values + live sil/aux.
- GLOW → `z = g·k`, sil/aux 0.
- SIL_ONLY with silhouette → `{1, (0,1,1,1), sil, aux}`; SIL_ONLY without silhouette → `{0, (0,1,1,1), 0, 0}`.
The previous draw's silhouette therefore can never leak to the next draw on the same program, whatever the slot sequence.
Uniform value caching (llglslshader.cpp:1779-1794) keeps repeats free. `noteUpload` is unchanged.

New slot `ALR_SLOT_SIL_ONLY` (lldrawpool.h:391 enum): `alrParams = (0,1,1,1)` (trims identity), `alrSil` / `alrSilAux`
live; explicit identity reset when the slot has no silhouette (above), so non-silhouetted styles render exactly as today.
Callers:
- `drawGeometryGhost` at llactormover.cpp:12159 (batches) and :12783 (static faces). `alr_on` keeps today's CLONE-only rule
  for the BEAUTY/GLOW slots; every other style (shared Actor FX replays, `mLightResponseKey` null) uploads
  `ALR_SLOT_SIL_ONLY` with keys `(gp.mLightResponseKey, di->mActorFxOwner, di->mActorFxFallbackOwner)`.
- `renderSystemActorGhost` (llactormover.cpp:15093): `ALR_SLOT_SIL_ONLY` with the avatar's actor id, to the bound
  actorghost program, after bind.
Path ghosts never resolve (no ALR key). There is also the debug-blend instrument (§4.3). No other call site changes: every avatar draw already
funnels here (ALR §5.3-§5.6).

### 5.4 Pipeline — `pipeline.cpp` / `pipeline.h`
- `addDeferredAttachments` (:680, :689) → RGBA; effective emissive predicate at :682 (§4.6).
- `uploadSilhouetteFrame(LLGLSLShader&)` (new, file-static): uploads `alr_sil_active = sSilFrameMask | (cover_valid ? 32 : 0)`
  and, when cover is valid, binds `silCoverMap` (`TFO_POINT`). It is called in `bindDeferredShader` after `shader.bind()`
  (:22999) **and** in the fast branch of `bindDeferredShaderFast` beside `uploadShadowUniforms` (:22949). Uniform uploads are
  value-cached, and programs without the uniform early-out.
- `silCover`: `ensureSilCover`, `renderSilCover(phase)`, release (§4.4); call sites :7841 (Phase A, before
  `doAtmospherics`) and :22198 (Phase B).
- No change to haze/froxel blend functions (:25275-25277, :25329-25331, froxel apply bind :20283).

### 5.5 Other C++
- **llreshadebridge.cpp:247:** do not publish attachment 3 under the silhouette-only emissive mode (§4.5).
- **llviewercontrol.cpp:1165-1166:** register `AvatarSilhouetteForceEmissive` with `handleEnableEmissiveChanged`.
- **llviewershadermgr.cpp:**
  - effective predicate + `EMISSIVE_SILHOUETTE_ONLY` at :491-497 and :1235-1240;
  - `HAS_EMISSIVE` (with the effective predicate) added AFTER `clearPermutations()` for `gFroxelApplyProgram` (:4867),
    `gDeferredProjectorVolumetricProgram` (:4131) and `gVisibleDiffuseSeedProgram` (:3299), never before it;
  - the new `gSilCoverProgram[6]` beside the Actor Mask programs (:5226-5262).
- **llactormover.{h,cpp}:** `renderSystemSilCover` beside `renderSystemActorMask` (h:755, cpp:15041).
- **settings.xml** (beside :29944): `AvatarSilhouetteDebugCarrier` (Bool 0, Persist 0), `AvatarSilhouetteForceEmissive`
  (Bool 0, Persist 0), `AvatarSilhouetteCoverBias` (F32 −8, Persist 1).

---

## 6. Shader changes
All avatar-program inserts are under `#ifdef HAS_ACTOR_FX` (and the file's `#ifndef IS_HUD` where present). Every new
branch is on `alrSilOn()` or the frame mask, so off-path pixels execute today's arithmetic.

### 6.1 Module `class1/alchemy/avatarLightResponseF.glsl`
The existing helpers, `alrGraphicCover` included (R2 reverts R1's k change), are **unchanged**. New code:
```glsl
uniform vec4 alrSil;      // rgb fill F (linear), a = a_q; 0 => off
uniform vec4 alrSilAux;   // x carrier, y haze over (1/0), z keep rim (1/0)
bool  alrSilOn()          { return alrEnabled == 1 && alrSil.a > 0.0; }
float alrSilKeep()        { return alrSilOn() ? 1.0 - alrSil.a : 1.0; }               // k
float alrSilRimKeep()     { return (alrSilOn() && alrSilAux.z < 0.5) ? 1.0 - alrSil.a : 1.0; }   // r
bool  alrSilHazeOver()    { return alrSilAux.y > 0.5; }
bool  alrSilNeedsFogZero(){ return alrSilOn() && !alrSilHazeOver(); }
vec3  alrSilPreFog(vec3 c, float w)               { return (alrSilOn() && alrSilHazeOver()) ? c + alrSil.a * alrSil.rgb * w : c; }
vec3  alrSilPostFog(vec3 c, vec3 fog_zero, float w){ return alrSilNeedsFogZero() ? c + alrSil.a * (alrSil.rgb * w - fog_zero) : c; }
vec3  alrSilFillAdd()     { return alrSil.a * alrSil.rgb; }       // call only under if (alrSilOn())
vec4  alrSilGBufferPbr(vec3 e)    { return vec4(e * (1.0 - alrSil.a) + alrSil.a * alrSil.rgb, alrSilAux.x); }   // under if (alrSilOn())
vec4  alrSilGBufferLegacy()       { return vec4(alrSil.a * alrSil.rgb, alrSilAux.x); }                          // under if (alrSilOn())
```
`alrGraphicCover` (:70-75) stays z-only: in R2 it operates on the unscaled styled result (§6.5), and k is applied once
afterwards.

### 6.2 Deferred G-buffer writers (only `frag_data[3]` changes)
- PBR (pbropaqueF.glsl:221, pbropaqueIndexedF.glsl:334): split with `#ifdef HAS_ACTOR_FX`; under it,
  `frag_data[3] = alrSilOn() ? max(alrSilGBufferPbr(emissive), vec4(0)) : max(vec4(emissive,0), vec4(0));`
  (silhouette-only mode: §4.6). The existing debug line (:223 / :336) stays after it.
- Legacy: inside each `HAS_EMISSIVE` + `HAS_ACTOR_FX` block, after the existing debug line:
  `if (alrSilOn()) frag_data[3] = alrSilGBufferLegacy();`, at class3 materialF.glsl:610 (non-BLEND),
  materialIndexedF.glsl:379, bumpF.glsl:140, diffuseF.glsl:120, diffuseIndexedF.glsl:123, diffuseAlphaMaskIndexedF.glsl:111,
  diffuseAlphaMaskNoColorF.glsl:107, avatarF.glsl:107. The Actor FX legacy emission written there today is never shown,
  because the softenLight legacy branch does not add `gb.emissive` (softenLightF.glsl:237-307). Dropping it keeps the
  fade continuous.

### 6.3 Deferred readers
New in **class1/deferred/deferredUtil.glsl**, after `getNormRaw` (:434-438):
```glsl
uniform int alr_sil_active;   // [AvatarSilhouette] frame mask, §5.2 bits
bool alrSilDecode(float carrier, float gbuffer_flag, out float k, out float r, out bool haze_skip);   // §4.2; k = 1 - a_q, r = rim_cut ? k : 1
```
The decode is gated by `alr_sil_active & 1` and the flag test, and lives under `HAS_EMISSIVE` (now always true while a
silhouette is active, §4.6).
- **softenLightF.glsl:**
  - decode after :155;
  - PBR (:219): emissive argument `alr_sil ? vec3(0) : colorEmissive`;
  - output (:314): `alr_sil ? clampHDRRange(color.rgb * final_scale * alr_k + gb.emissive.rgb) : <today>`;
  - silhouette-only rule at :157 / :226-236 (§4.6);
  - debug (bit3): `frag_color.rgb = vec3(0,1,1)*(0.25 + 0.75*(1-alr_k))` on valid pixels.
- **multiPointLightF.glsl** (decode after :85), **pointLightF.glsl** (after :83), **spotLightF.glsl** (after :149):
  - invariant: output `= k·(non-rim) + r·rim`;
  - where `rim_color` is added (multiPoint :204, point :137 and :184, spot :326):
    `if (alr_sil) final_color = final_color * alr_k + rim_color * alr_r; else final_color += rim_color;`;
  - pixels that reach the output without the rim block (`rigRimMode()==0`) get `final_color *= alr_k` before the output
    write (multiPoint :209, point :195, spot :332);
  - the rim keeps `diffuseColor` / `diffuse` from the unscaled G-buffer, so its tint is preserved;
  - actor-only light programs reuse these files (llviewershadermgr.cpp:2761-2762).
- **hazeF.glsl:** `uniform sampler2D silCoverMap;`. Inside `if (do_atmospherics)` after the cine fold (:116), when bit5 is set:
  `float h = texture(silCoverMap, tc).r; color *= 1.0 - h; alpha = 1.0 - (1.0 - h)*(1.0 - alpha);`.
- **waterHazeF.glsl:** same on `fogged` before :74.
- **froxelApplyF.glsl:** same on `(L, T)` before :82.
- **class3 projectorVolumetricF.glsl:** `vec2 sc = texture(silCoverMap, tc).rg`; before :734 `shaft *= 1.0 - sc.r`
  (in-air scatter, RESOLVED-3); at :805 `rim *= 1.0 - sc.g` (projector surface rim follows Keep Rim).
- **visibleDiffuseSeedF.glsl:** after :74, for valid carriers `diffuse *= alr_k`, so SL_VISIBLE_DIFFUSE (and RTGI through
  the provider's sidecar branch, SL_GBufferProvider.fx:565) sees the silhouetted diffuse.

### 6.4 Coverage shader `class1/deferred/silCoverF.glsl` (new)
Derived from actorMaskF.glsl: `mirrorClip`, authored-alpha lookup, `mask_cutoff` discard for masked passes. Output
`vec4(sil_cover.x*cov, sil_cover.y*cov, 0, cov)`; `sil_blended` selects fractional coverage.

### 6.5 Forward lit paths
- **deferredUtil.glsl:**
  - (R3 P0-1) The body of `pbrCalcPointLightOrSpotLightRimKeep` (:930-988) moves to
    `void pbrCalcPointLightOrSpotLightRimSplit(..., vec4 rim, float spec_keep, out vec3 nonrim, out vec3 rim_raw, out bool rim_on, out float light_scale)`.
    It computes exactly today's intermediates and applies **no** final scale:
    - `nonrim = intensity·clamp(nl·(diffPunc + specPunc·spec_keep), 0, 10)` (:961-969), set to 0 under debug mode 2 (:977-980);
    - `rim_on = (dist ≤ 1 && rig_rim_mode != 0)`, `rim_raw = rim_col` (:973-982) or 0;
    - `light_scale` = today's `final_scale` (:984-986).
    The old `RimKeep(...)` becomes `RimSplit(...); vec3 c = nonrim; if (rim_on) c += rim_raw; return c * light_scale;`,
    the same operations in the same order as today (`color = …; color += rim_col; return color * final_scale;`).
  - Silhouette callers: `alrSilOn() ? (nonrim * k + rim_raw * r) * light_scale : RimKeep(...)`, so the off path runs
    today's function unchanged.
  - Parity: the pre-refactor body is kept verbatim as `pbrCalcPointLightOrSpotLightRimKeepRef` under
    `#ifdef ALR_RIMSPLIT_PARITY` only (T21); default builds do not compile it.
- **pbralphaF.glsl:**
  - after :325, `color *= alrSilKeep();` (base, IBL and emission are inside);
  - LIGHT_LOOP :332 → under `alrSilOn()`: `RimSplit(…, rig_rim_lights[i], alr_keep, nr, rr, ron, ls); light += (nr * alr_sk + (ron ? rr : vec3(0)) * alr_rk) * ls;`
    else today's `light += …RimKeep(…)`, with `alr_sk = alrSilKeep()`, `alr_rk = alrSilRimKeep()`; non-avatar programs
    keep the macro verbatim (`#ifdef HAS_ACTOR_FX`);
  - before :344 `color = alrSilPreFog(color, w)`; after :344 the fog-zero call and `color = alrSilPostFog(color, fz, w)`;
  - `HAS_VISIBLE_DIFFUSE` publishes `diffuseColor * alrSilKeep()` (:355).
- **sharedActorFxPbrF.glsl** (R2 P0-1: the post-light treatment is nonlinear, so k goes AFTER it):
  1. The base (:582-586) stays unscaled.
  2. LIGHT_LOOP (:591-596): under `alrSilOn()`, call `RimSplit` and accumulate
     `light += (ron ? nr + rr : nr) * ls` (today's value and order) and `alr_rim += (ron ? rr : 0) * ls`; otherwise
     today's macro verbatim.
  3. `color += light` (:607) as today, i.e. lit incl. rim.
  4. `alr_lit_pre = color` (:610); `color = actorFxPbrPostLight(color, …)` (:611-615);
     `color = alrGraphicCover(color, alr_lit_pre, σ)` (:617, z only). This is today's styled colour `S` without emission.
  5. If `alrSilOn()`:
     - `ΔR`:
       - if `alrSilRimKeep() == 1.0 && any(alr_rim > 0) && actorFxActive()`: a second evaluation
         `S_nr = alrGraphicCover(actorFxPbrPostLight(alr_lit_pre − alr_rim, …, dummy_emission), alr_lit_pre − alr_rim, σ)`
         and `ΔR = S − S_nr`;
       - else if `alrSilRimKeep() == 1.0` (no look active): `ΔR = alr_rim`, exact, because postLight returns `lit_color`
         when inactive (actorFxF.glsl:1492-1495) and `alrGraphicCover` is identity for σ = 0;
       - else `ΔR = 0`.
     - Then `color = color·k + ΔR·(1 − k)` when Keep Rim is on, or `color·k` when it is off.
  6. :620 `color += alrSilKeep() * (actor_fx_authored_emission + actor_fx_synthetic_emission)`.
  7. Pre/post fog around :622-623.
  - At k = 0 with Keep Rim off the result is exactly 0 + fill, whatever the look does.
  - With Keep Rim on it is the rim's styled contribution. `ΔR` may be negative for inverting looks; the final `max(…,0)`
    at :637 clamps.
  - The second post-light call is gated (Keep Rim on, look active, rim present) and its synthetic emission is discarded.
    `actorFxPbrPostLight` is time- and fragment-deterministic within a fragment (time and `frag_coord` inputs only,
    actorFxF.glsl:1564-1578), so `S − S_nr` isolates the rim.
- **alphaF.glsl** (legacy alpha incl. avatar alpha/hair):
  - the local `calcPointLightOrSpotLight` (:110-209) gains `float sil_keep, float rim_keep`: non-rim × sil_keep and rim
    × rim_keep, before its own classic 0.9 (:204-207);
  - after :368 `color.rgb *= alrSilKeep()`;
  - :386 `+= alrSilKeep()*actor_fx_emissive`;
  - LIGHT_LOOP :373 passes the keeps;
  - pre/post fog around :389;
  - visible diffuse (:410) × k.
- **class3 materialF.glsl (BLEND):**
  - its local `calcPointLightOrSpotLight` (:142-…) gets the same two keeps, and its `glare` contribution × sil_keep;
  - after the env block (:533) `color *= alrSilKeep(); glare *= alrSilKeep();` (glare is reflection-driven alpha);
  - :557 emission × k;
  - LIGHT_LOOP :539;
  - pre/post fog around :561;
  - visible diffuse (:584) × k.
- `w = classic_mode > 0 ? 1.0/1.1 : 1.0`.
- Fog-zero: `vec3 fz = vec3(0); if (alrSilNeedsFogZero()) fz = applySkyAndWaterFog(pos, additive, atten, vec4(0,0,0,1)).rgb;`.
- Non-avatar programs compile the same files with literal 1.0 keeps (`#ifdef HAS_ACTOR_FX` locals).

### 6.6 Unlit forward and ghost
- **fullbrightF.glsl:**
  - after :159 `color.rgb *= alrSilKeep()`;
  - IS_ALPHA: pre/post fog around :170 with `w = 1`;
  - opaque: `if (alrSilOn()) color.rgb += alrSilFillAdd();` before the debug override (:176-179). Haze is then applied by
    hazeF with the coverage mask (§4.4).
- **fullbrightShinyF.glsl:** after `applyLegacyEnvKeep` (:154) `color.rgb *= alrSilKeep()`; fill add before :170.
- **actorghostF.glsl** (R2: output-side for EVERY `ghostLook`, after all look processing):
  - declare `alrSil` and `alrSilAux` beside :118-119; `bool sil = alrEnabled == 1 && alrSil.a > 0.0`;
  - the ALR block :1485-1495 is unchanged (CLONE Brightness/Glow);
  - **world pass** (`GHOST_WORLD_PASS`):
    - `F' = ghostWorldLinear != 0 ? F : alr_ghost_l2s(F)`;
    - immediately before the fog branch (:1542): `if (sil) rgb *= k;` — after signal modulation (:1502), Dissolve
      (:1503-1507) and `worldRadiance` (:1511), so every look contribution is scaled;
    - Tron Suit Layer branch (:1542-1556) is additive neon over the (silhouetted) native beauty: `× k` only, no fill;
    - else branch (:1557-1561): Haze ON adds `a·F'` before the fog call, Haze OFF adds `a·(F' − fogZero)` after it
      (`fogZero` as at :1525-1527);
    - glow-only path: `glow *= k` at :1533 (look/synthetic bloom vanishes);
  - **late display compile** (no fog): before the debug override (:1564), `if (sil) rgb = l2s(s2l(rgb)·k + a·F)`;
  - SWEEP_GLOW gets `z = g·k` and no fill (§5.3); the debug override (:1564) stays last;
  - upload: CLONE draws use the BEAUTY/GLOW slots, all other styles `ALR_SLOT_SIL_ONLY` (§5.3).

### 6.7 Glow (silhouette has no glow: authored and look)
Final writes `× alrSilKeep()`: emissiveF.glsl:102, emissiveIndexedF.glsl:182, pbrglowF.glsl:156, pbrglowIndexedF.glsl:220,
sharedActorFxPbrGlowF.glsl:318, sharedActorFxPbrSyntheticGlowF.glsl:209-210, actorFxGlowF.glsl:64, actorFxPbrGlowF.glsl:61.
All are `hasActorFx` (llviewershadermgr.cpp:2135, :2161, :2373, :3197, :3218, :3239, :3263) and receive the per-draw upload.

### 6.8 Not changed
- Shadow/depth/velocity and every `hasActorFxShadow`-only program: the silhouette casts its normal shadow.
- impostorF and the alpha-impostor programs; tonemap/colorCorrect; reflectionProbeF.
- `rigRimTerm`, `pbrIbl`, `pbrPunctual`, `pbrBaseLightKeep` bodies; the G-buffer flag encoding; HUD variants.
- avatar/avatarF.glsl, avatar/eyeballF.glsl, objects/simpleF.glsl (RESOLVED-6).

---

## 7. UI — `floater_avatar_light_response.xml` + `alfloateravatarlightresponse.{h,cpp}`

### 7.1 Layout
Flat floater with no scroll document, so ALScrollFit is not needed. Insert after `chk_bypass` (xml:144-153):
| top | Widget |
|---|---|
| 342 | `text txt_sil_header` "Silhouette" (h 16) |
| 360 | `slider sld_sil_amount` "Silhouette (%)" 0–100, increment 1, label_width 96, width 340; tooltip "Blends this avatar from its normal look (0 %) to a flat, unlit fill (100 %). The fill is scene light: exposure, tonemap and grade still apply. For exact key colours use Actor Chroma Key." |
| 384 | `text` "Fill" (w 90) · `color_swatch swatch_sil_fill` (left 106, w 28, h 22, label "", can_apply_immediately true) · `combo_box combo_sil_fill` (left 140, w 210): Black 0, Charcoal 1, White 2, Chroma Green 3, Chroma Blue 4, Custom 5 |
| 410 | `check_box chk_sil_haze` "Haze over silhouette" (w 170) · `check_box chk_sil_keep_rim` "Keep Rim" (left 186, w 164) |
| 432 | `text txt_sil_note` (amber, h 16, hidden): "Preparing silhouette buffer…" (§4.6), or "Keep Rim: Rig Rim is off (Cine Light Rig)" when `CineRigRimEnabled` is false |

Shift everything from `combo_preset` (xml:154) down by 110; floater `height` 470 → 580. Add the combo item "Silhouette"
(value 8) after xml:193.

### 7.2 Behaviour
- Amount slider: like the other sliders (cpp:102-114): commit → apply without saving; mouse-up → save. The wheel is
  viewer-wide (rule 9); wheel commits are preview-only and the close flush (`onClose`, cpp:155-159) saves.
- Swatch: preview/commit apply to all selected rows; select/close saves. The fill combo applies the preset and saves.
  `refreshEditors` shows `matchFillPreset` via `setValueIfChanged`.
- Checkboxes apply and save (pattern of `onBypass`, cpp:620-633).
- The new controls are enabled with the edit group. Haze, Rim and Fill stay editable at amount 0.

### 7.3 CLAUDE.md UI rules (the floater is modified, so it is brought into compliance)
- Tracy zone `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` in `draw()` (cpp:161).
- `refreshRows` and `refreshStatus` (cpp:310-463; `deleteAllItems` / `addElement` / `setText`) move out of `draw()` to a
  `gIdleCallbacks` function (registered in `postBuild`, removed in the destructor). It runs at 1 Hz only while
  `isWidgetOnScreen(this)` (aluirefresh.h:510); `onVisibilityChange(true)` forces an immediate refresh.
- `draw()` only checks the timer and the revision gate (`ALRevisionGate`, aluirefresh.h:389, on
  `ALAvatarLightResponse::revision()`), and uses `setTextIfChanged` / `setVisibleIfChanged`.

---

## 8. Interactions
| System | Behaviour |
|---|---|
| Diffuse / Tame / Brightness / Glow | applied first (G-buffer and helpers unchanged); the silhouette scales the result |
| Bypass / debug tint | bypass = identity incl. silhouette; debug tint overrides and disables the silhouette |
| Inherit / worn animesh / entity clones | unchanged ALR chains (ALR §3) |
| Actor FX looks | Pre-light looks (actorFxApply, every material writer and forward shader) are inside Lit, which is scaled at the output. The PBR post-light treatment (sharedActorFxPbrF) is evaluated unscaled and its output × k (R2). actorghost replays (shared proxies, system bodies) get `k·styled` via `ALR_SLOT_SIL_ONLY`. Look emission and glow × k ⇒ nothing of any look remains at 100 %; Keep Rim keeps only the isolated styled rim delta |
| Rig Rim / projector rim | Keep Rim ON: identical to today at every amount (unscaled tint, rim × 1); OFF: × k. An avatar not lit by a rim-carrying rig light or rim projector gets no rim; Keep Rim never synthesises one |
| Actor-only lights | same light shaders |
| Shadows | cast unchanged; received shadows disappear with k |
| Probes / mirrors / water reflection | same per-draw uniforms and passes |
| ReShade | §4.5 (bridge unchanged); RTGI gets the k-scaled diffuse via the sidecar, the raw albedo without it (L4) |
| Actor Chroma Key | independent (its mask programs are not `hasActorFx`); merge touch point is pipeline.cpp only, different functions |
| Actor Mask | read-only reuse of `ScopedActorMaskState` / `kActorMaskPasses`; its code is untouched |

---

## 9. Cost (restated, R1 P1-5)
- **Feature unused:** no added GL calls, texture binds, render targets or fullscreen fetches. Per fragment:
  - one uniform `int` compare (`alrEnabled`) in avatar programs;
  - one uniform `int` compare (`alr_sil_active`) in softenLight, the light passes, haze, water haze, froxel apply,
    projector march and the seed pass.
  - The emissive attachment's alpha store: probably free with padding, ≤ 2 B/px.
  - Measured in T16. A per-feature permutation was rejected because it doubles every avatar program.
- **Active:**
  - G-buffer carrier decode, ~10 ALU per lit pixel per pass;
  - with Haze OFF or Keep Rim OFF, the coverage replay (Phase A always, Phase B when volumetrics run) and one point fetch
    in each fog/volumetric pass;
  - forward Haze OFF draws: one extra `applySkyAndWaterFog`;
  - two extra `uniform4fv` per adjusted draw;
  - vintage mode only: a one-time shader rebuild (§4.6).

---

## 10. Tests

### 10.1 Unit (TUT, `tests/alavatarlightresponsemodel_test.cpp`; 8 today → 29)
1. Defaults (amount 0, black, haze true, rim false), `isIdentity(Params())`, `packAll(...).mSil == 0`.
2. `isIdentity` is false at amount 0.5, true with bypass and at amount 5e-5.
3. `sanitize` clamps and replaces non-finite values.
4. LLSD round-trip; an old map gives silhouette off; a malformed `sil_fill` gives black.
5. `packAll` fields; bypass gives zero silhouette fields.
6. Carrier, HDR: for every q ∈ 1..510 × 4 flag sets, `silDecode` returns q and the flags exactly after fp16 emulation
   (11-bit mantissa). 1.0, 0, 0.5, 1.75 and 2048.5 decode invalid. 2048 is the maximum value.
7. Carrier, LDR: for every q ∈ 1..62 × 4 flag sets, exact through unorm8; codes 0 and 249-255 are invalid.
8. `silQuant` / `silAmountQ`: |a_q − a| ≤ 1/1020 (HDR) and 1/124 (LDR); a = 1 gives a_q = 1 exactly.
9. Fill preset codes; `matchFillPreset` (exact match and Custom).
10. Presets: SILHOUETTE keeps the other fields; RESET clears; GLOW_OFF keeps the silhouette.
11. `silForward`, Haze ON = `k·fog(L) + a·fog(F·w)`; 12. Haze OFF = `k·fog(L) + a·F·w` (1e-6); 13. a = 0 returns `fog(L)`
    bitwise; a = 1 with Haze OFF returns `F·w`.
14. `silHazeBlend`: h = 0 is identity bitwise; h = 1 gives `(0, 1)`.
15. Metallic mid-fade (CPU mirror of `pbrPunctual` + `calcDiffuseSpecular` from deferredUtil.glsl:726-781, :1026-1032):
    output-side scaling equals `k·Lit` for m ∈ {0, .5, 1}, k ∈ {.25, .5, .75}; input-side scaling does not (regression guard).
16. Keep Rim: the CPU mirror of `rigRimTerm` with r = 1 is identical for every k (tint uses the unscaled diffuse).
17. `graphicCover` unchanged (existing ALR test still passes).
18. Nonlinear-look regression (CPU mirror of the shape of `mix(layer_source, fx(source), σ)` with an fx that is non-zero at
    source = 0, e.g. the Ghost palette branch actorFxF.glsl:1581-1585): output-side `k·S` is 0 at k = 0; the R1 input-side
    order is non-zero (guard).
19. Rim delta: with an identity "look", `ΔR == Rim` bitwise; with a nonlinear look, `k·S + (1−k)·ΔR` equals `S` at k = 1
    and `ΔR` at k = 0.
20. `uploadPlan` full reset: the sequence BEAUTY(sil) → SIL_ONLY(no sil) → nullptr → GLOW → SIL_ONLY(sil) → BEAUTY(no sil)
    sets all four fields in every step. The no-silhouette steps yield `enabled == 0`, `sil == 0`, `aux == 0`; no field is
    "unchanged".
21. RimSplit recombination mirror (CPU float): for random `nr, rr, fs` with `rim_on` true/false,
    `(rim_on ? nr + rr : nr) * fs` is bitwise equal to the reference `color = nr; if (rim_on) color += rr; color * fs`.
    The distributed form `nr·fs + rr·fs` is NOT bitwise equal for at least one sampled case (guard against regression to R2).

### 10.2 In-world (user runs; expected outcomes stated in advance)
Setup as ALR §12.2: clear the shader cache once, freeze EEP and pose, fixed camera, DebugLog on.
- **T0 OFF parity:** old vs new exe with no entries are within the old-vs-old floor. A Tame-only entry renders identically
  to the old build. Amount 0 with Haze/Rim OFF renders identically.
- **T1 Debug tint:** unchanged ALR T1 coverage, with a silhouette set.
- **T2 Carrier instrument** (`AvatarSilhouetteDebugCarrier`): deferred surfaces of the target turn cyan; hair/fullbright do
  not (forward); nothing else is cyan. No `SIL-CARRIER-BLENDED` lines.
- **T2b Coverage instrument** (same setting, mask debug view): the whole visible avatar incl. hair (proportional) and
  fullbright pieces shows in the mask. A world object in front of the avatar is NOT marked.
- **T3 Fade 0 → 100:** smooth, no pop; at 100 % one flat colour lit and shadowed. **Shiny metal (PBR m = 1) and glossy
  legacy patches fade linearly**: at 50 % the reflection is half its original strength, not a quarter.
- **T4 Black:** 100 %, Haze OFF, Rim OFF; tonemap types 0-7 with neutral grade read (0,0,0) ±1. Type 8 gives the log black code.
- **T5 Chroma:** Chroma Green, Haze OFF, dynamic exposure off, RenderExposure 1, tonemap mix 0 or the no-post path, neutral
  grade, bloom off ⇒ #00B140 ±1. With a tonemap: a different but constant code.
- **T6 Haze:** subject at 100 m in WindLight haze, Cine Haze, froxel fog and projector shafts.
  - Haze ON: it fades like the background at that distance.
  - Haze OFF: flat fill everywhere, **including the hair, fullbright pieces and the pixels where shafts cross the subject**.
  - A world fullbright sign in front of the subject is still hazed. Underwater: same with water fog.
  - Repeat with a GhostDeferredEnable proxy clone and with a resident wearing a shared Actor FX look: identical outcome.
- **T6b Phase-B matrices:** froxel fog on, On-lens filters (Graduated ND) and HDR bloom on, camera moving. Haze-OFF hair
  coverage (T2b view) stays locked to the hair every frame — no offset, scale or one-frame lag — and froxel fog never
  appears on the hair strands.
- **T7 Keep Rim:** rig rim light behind the subject.
  - ON: the rim is pixel-identical to the unsilhouetted rim at 0/50/100 %.
  - OFF: it fades with the amount.
  - Same for a projector rim. With no rig rim: no difference, and the note shows.
- **T8 Glow:** authored, PBR emissive and Actor FX look glow vanish at 100 % and are halved at 50 %.
- **T9 Shadow:** cast unchanged. **T10 Hair edges:** blend fill over sky by alpha, no lit fringe.
- **T11 Clones:** an entity clone and a proxy clone (GhostDeferredEnable) behave exactly like an avatar, incl. Haze OFF under
  WindLight haze, froxels and shafts (T2b coverage view shows them). A Studio overlay clone silhouettes; Haze OFF holds
  because the overlay draws after all fog passes, and Haze ON has no effect on overlays (§3.3). A worn pet follows its wearer.
- **T19 Actor FX looks:** resident with (a) a PBR Layer look (e.g. Ghost, Hologram), (b) a PBR graphic Cover look, (c) a
  legacy Cover look replayed through actorghost, (d) Tron Suit Layer.
  - At 0 %: identical to today.
  - At 50 %: half-faded styled look.
  - At 100 %: flat fill, no trace of the look's colour, emission or glow.
  - Keep Rim ON at 100 %: only the rim edge (styled by the look) remains.
- **T21 RimSplit parity** (`ALR_RIMSPLIT_PARITY` permutation via debug setting `AvatarSilhouetteRimParity`, Persist 0):
  - pbralphaF/sharedActorFxPbrF evaluate both the reference body and `RimKeep(RimSplit)` per light and paint magenta
    where any channel differs bitwise (`floatBitsToUint`);
  - with Rig Rim on, rim debug mode 0/1/2, classic and non-classic skies: no magenta pixel;
  - log `[ALR] RIMSPLIT PARITY PASS|FAIL`.
- **T22 Phase-B / hook state invariant:** `ScopedSilCoverInvariant` (debug only, under `AvatarSilhouetteDebugCarrier`)
  snapshots the following before and after each phase and hook:
  - blend enable, factors and equations; stencil enable; depth test/mask/func; cull enable/mode; polygon-offset enable,
    factor and units; draw and read FBO; draw buffers; viewport; scissor;
  - `gGLModelView` / `gGLProjection` / `gGLLastMatrix`; both gGL matrix stacks; bound shader; active texture unit;
    LLRender's cached blend factors.
  It logs `[ALR] SILCOVER STATE OK` or `[ALR] SILCOVER STATE-LEAK <field>`. Expected: OK every frame with Haze OFF, hair,
  proxy clones, a shared-look resident, bloom, on-lens filters and the visible-diffuse sidecar on.
- **T23 Static proxy coverage / stale queue:** a proxy clone with rigid (static) attachments, including one alpha-blended
  attachment, in Haze OFF fog:
  - the attachments are covered (T2b view) and unhazed;
  - the 1 Hz log reports `SILCOVER proxies=N batches=B statics_A=S1 statics_B=S2` with S1, S2 > 0;
  - opening a reflection probe update, a HUD and a Prism lens: `SILCOVER-SKIP view` / `stale-queue` counters increase and
    nothing asserts.
- **T20 Post-bloom coverage:** as T6b, with the bloom pyramid feeding back (BDMergeProjectorVolumetricsBloomFeed > 0):
  shafts crossing the subject never brighten the Haze-OFF fill.
- **T12 Persistence / back-compat / Director scenes.** **T13 Bypass / master.**
- **T14 Vintage mode:** with emissive and HDR off, setting a silhouette triggers one rebuild ("Preparing…"). After it, the
  fill, Haze and Rim work; non-silhouetted pixels are byte-identical to a vintage capture taken before.
- **T15 UI:** no `ALFloaterAvatarLightResponse::draw` spikes with 50 rows; slider wheel and list scroll behave; a hidden floater does no work.
- **T16 Perf:** Tracy GPU zones (G-buffer, softenLight, haze, froxel) and the VRAM readout, feature unused: within noise of
  the old build. Feature active: report the coverage-pass cost.
- **T17 ReShade:** SL_BridgeDebug EMISSIVE cell has the same state before and after the build (valid in normal mode, invalid
  in vintage and forced vintage). Incumbent effects (RTGI, GBufferProvider) look identical with no silhouette.
- **T18 Targets:**
  - resize the window, toggle HDR, toggle the emissive buffer, take a tiled high-res snapshot, open a hero mirror and a Prism lens;
  - no FBO-incomplete or allocation warnings;
  - ghost-deferred diagnostics readback of attachment 3 reports RGBA16F;
  - `silCover` allocates only while Haze OFF / Rim OFF silhouettes exist.

---

## 11. Proof obligations (reviewers)
1. With `sSilFrameMask == 0` and no amount > 0, every edited shader executes today's arithmetic (branches only).
2. No pass decodes a carrier the writer did not set (flag gate, integrality, range, clear = 1.0).
3. Avatar G-buffer writes are unblended (§4.3 citations); T2 instrument clean.
4. k is applied exactly once per term on every path; the rim gets r and never k (Keep Rim ON).
5. Binary cache: `LLGLSLShader::hash()` hashes only a program's own files (ALR §11.4). Every program that calls a new or
   changed module/deferredUtil function has an edited main file (§6 lists them). The `RimKeep` refactor keeps the old
   names as exact forwarders. T0 clears the cache.
6. Global GL state: the only new state changes are inside `renderSilCover`, `renderGhostSilCover` and
   `renderSharedProxySilCover`:
   - blend enable, factors and equations, stencil enable, depth test/write/func, cull, polygon offset, FBO, draw
     buffers, viewport, matrices;
   - all wrapped in `ScopedActorMaskState` + `ScopedSilCoverState` (§4.4), with LLRender's blend cache restored through
     gGL;
   - verify against (a) the beauty pass incl. the armed visible-diffuse guard and (b) the feature-off path, where no
     hook is entered (CLAUDE.md rule 1);
   - T22 measures it.
7. Every `HAS_EMISSIVE` permutation added after `clearPermutations()`; `EMISSIVE_SILHOUETTE_ONLY` world output equals vintage (T14).
8. HUD variants never run silhouette code.

---

## 12. Work packages
Sequential. Codex implements each via `--prompt-file`, a fresh Codex reviews each, then the cross-package adversarial
review, then ONE build.

| WP | Scope | Files (exhaustive) |
|---|---|---|
| WP1 | Model + tests incl. `uploadPlan` and the RimSplit/rim-delta/nonlinear-look mirrors (§5.1, §5.3, §10.1) | indra/newview/alavatarlightresponsemodel.h/.cpp; indra/newview/tests/alavatarlightresponsemodel_test.cpp |
| WP2 | Store, upload (incl. `ALR_SLOT_SIL_ONLY`), pipeline frame upload, RGBA format, forced emissive, ReShade publish guard, settings (§4.1, §4.5, §4.6, §5.2-§5.5 except silCover) | indra/newview/alavatarlightresponse.h/.cpp; lldrawpool.h (:391 enum); lldrawpool.cpp (`upload_alr` :467-490 restructured around `uploadPlan`, full reset every call); pipeline.cpp (:680-697, `uploadSilhouetteFrame` + :22949/:22999 calls); llreshadebridge.cpp (:247); llviewercontrol.cpp (:1165 area); llviewershadermgr.cpp (:491-497, :1235-1240, :3299, :4131, :4867); app_settings/settings.xml |
| WP3 | Module + G-buffer writers + deferred lighting readers + seed (§6.1-§6.3 lighting part) | app_settings/shaders/class1/alchemy/avatarLightResponseF.glsl; class1/deferred/{pbropaqueF, pbropaqueIndexedF, pbrterrainF, materialIndexedF, bumpF, diffuseF, diffuseIndexedF, diffuseAlphaMaskIndexedF, diffuseAlphaMaskNoColorF, avatarF, deferredUtil (decode + uniform only), visibleDiffuseSeedF}.glsl; class3/deferred/{materialF (non-BLEND), softenLightF, multiPointLightF, pointLightF, spotLightF}.glsl |
| WP4 | Coverage mask engine + ghost coverage hook + matrix contract + consumers (§4.4, §6.3 fog part, §6.4) | indra/newview/pipeline.h; pipeline.cpp (`ensureSilCover`, `renderSilCover` incl. the Phase-B matrix/viewport save-load-restore using :22045-22046, release, call sites :7841 and :22198, `silCoverMap` binding in `uploadSilhouetteFrame`, `LLPipeline::renderGhostSilCover(phase)` beside :6268 using `ScopedGhostTransform` :6042 / `classifyGhostBatchPhase` :6105 with the :7066-7081 guards, batches + `mStaticFaces`, `ScopedSilCoverState`, debug `ScopedSilCoverInvariant`, the 1 Hz SILCOVER counters); llviewershadermgr.cpp (`gSilCoverProgram`, beside :5226-5262) + its declaration (llviewershadermgr.h beside `gActorMaskProgram`, :181-183); llactormover.h/.cpp (`renderSystemSilCover`, `renderSharedProxySilCover(phase)`, `GhostDrawParams::mSilCoverPhase` at h:1015-1040, coverage mode in `drawGeometryGhost` :11962/:12085/:12615/:12955 (no sweep blend calls in coverage mode), shared-proxy re-submission from :17955); class1/deferred/silCoverF.glsl (new); class3/deferred/{hazeF, waterHazeF, projectorVolumetricF}.glsl; class1/deferred/froxelApplyF.glsl; app_settings/settings.xml (`AvatarSilhouetteCoverBias`) |
| WP5 | Forward + glow + ghost + Actor FX output-side (§6.5-§6.7) | indra/newview/llactormover.cpp (`ALR_SLOT_SIL_ONLY` uploads at :12159, :12783 and in `renderSystemActorGhost` :15093); class1/deferred/deferredUtil.glsl (`RimSplit` raw-component refactor + `ALR_RIMSPLIT_PARITY`-gated reference body); llviewershadermgr.cpp (the parity permutation on the pbralpha / sharedActorFxPbr programs, added after `clearPermutations()`, only while `AvatarSilhouetteRimParity` is set); app_settings/settings.xml (`AvatarSilhouetteRimParity`, Persist 0); class2/deferred/{pbralphaF, sharedActorFxPbrF, alphaF}.glsl; class3/deferred/{materialF (BLEND), fullbrightShinyF}.glsl; class1/deferred/{fullbrightF, emissiveF, emissiveIndexedF, pbrglowF, pbrglowIndexedF, sharedActorFxPbrGlowF, sharedActorFxPbrSyntheticGlowF, actorFxGlowF, actorFxPbrGlowF}.glsl; class1/interface/actorghostF.glsl |
| WP6 | UI (§7) | indra/newview/alfloateravatarlightresponse.h/.cpp; skins/default/xui/en/floater_avatar_light_response.xml |

**OFF-LIMITS (every WP):** anything not listed. In particular: renderActorMask / drawActorMaskBatch / Actor Mask state
and shaders (read-only reuse only); llshadermgr.cpp flag encoding; the `rigRimTerm` / `pbrIbl` / `pbrPunctual` /
`pbrBaseLightKeep` bodies; reflectionProbeF; tonemap / colorCorrect / blitWithEffects; actorFxF / actorFxFallbackF;
shadow/depth/velocity shaders; impostor shaders; llvoavatar.cpp; the ReShade add-on (`reshade-addon/`); Actor Chroma Key;
the `enve` tree. Codex never builds.

---

## 13. Formerly OPEN — all RESOLVED (user away; standing instruction: take the recommendation)
- **RESOLVED-1** Keep Rim default **OFF**.
- **RESOLVED-2** Exact on-screen keying is deferred to Actor Chroma Key; no tonemap inversion here.
- **RESOLVED-3** Haze OFF also removes projector shafts and froxel fog over the silhouette (§6.3).
- **RESOLVED-4** No timed fade and no Director keyframing in this package; the Amount slider is the fade.
- **RESOLVED-5** Charcoal = **#262626**.
- **RESOLVED-6** avatar/avatarF.glsl, avatar/eyeballF.glsl and objects/simpleF.glsl (llviewershadermgr.cpp:5406, :5431, :5323)
  stay untouched unless T1/T2 show an uncovered visible surface.
