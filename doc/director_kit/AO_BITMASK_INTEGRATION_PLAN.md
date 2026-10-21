# Native XeGTAO framework + visibility-bitmask AO: SL integration plan

## Revision 3 (10/21)

- Re-review found **no P0**. This revision addresses P1-A/B/C and the P2 findings;
  Revision 2 below is historical and is superseded where these policies differ.
- Native capability uses static GL/extent inputs only. Added machine-level **Prepare
  native AO**, default OFF, a separate latched preparation/admission sequence, and
  zero native allocation/compilation for stock users who leave preparation OFF.
- Defined white initialization and per-view light-map content validity, including
  reserved-but-unwritten OFF, debug consumers and DOF scratch reuse.
- Sized candidate presets from an explicit CPU variance model before shader tuning;
  Low/Balanced have disclosed relaxed plane gates, High/Cinematic retain sigma ≤0.005.
  Revised gather/filter costs, budgets and contact-pan excess-delta measurement.
- Use capture slot order only; give the existing capture governor sole ownership of
  feed resolution/cadence, pinned during a take. Raster counts are report-only;
  content changes cannot freeze a scheduled feed. Clarified the gather-only 160 M cap
  and denoise accounting under the combined 2,000 M tap-equivalent ceiling.
- Corrected legacy gloss/environment citations to `softenLightF.glsl:341` / `:351`.

## Revision 2 (10/21)

- P1 now reserves the existing `deferredLight` whenever native AO is capable (option b),
  repairs both allocation/admission gates and the sun dispatch gate, and explicitly
  excludes preview/cube views. Prepared runtime toggles allocate nothing and reload no shaders.
- Added a three-pass spatial denoise minimum without TAA, revised Low/Balanced costs,
  and hard flat-plane variance and sub-pixel camera-pan gates for hardware recording.
- Defined classic-sky direct equality before nonlinear recombination; designed separate
  legacy gloss/environment probe hooks before SSR/hero replacement.
- Added combined AO + Relight + VCam resident/peak-memory and work admission, with
  main reservations, fixed feed allowances and deterministic feed-first degradation.
- Added HUD, hero mirror, impostor, water-reflection and DOF scratch policies/tests;
  made the stock-noise test hook production-inert and corrected shipped MIT notice paths.
- Counted startup shader companions/cache impact, corrected the two source citations,
  and raised P1–P5 effort to **40–60 engineer-days**, including P1 light-map work.

Date: 2026-10-21. **Planning only; no implementation, build, commit or stash.**
Audited branch: `feature/director-kit-p0`, HEAD `e0cc289b108f9acd3fe625fd7540ba6f6c79eb80`.
Source references below describe this checkout, not the older line numbers in the Relight brief.
Paths starting with `class1/`, `class2/` or `class3/` are relative to
`indra/newview/app_settings/shaders/`; other shortened viewer filenames are relative to
`indra/newview/`. Line references identify the start of the relevant implementation.

## 1. Recommendation and scope

Implement a native current-frame ambient visibility pass: XeGTAO-style slice setup,
depth preparation and spatial denoising first, then replace the two horizon extrema with
finite-thickness visibility sectors. Add bent-normal probe occlusion after scalar AO is
validated. Keep the stock SSAO branch available throughout rollout.

The user-visible goal is stronger, cleaner contact and cavity definition throughout the
world, especially around railings, fingers, lace and foliage. This cannot supply every
kind of game shadow: AO controls nearby **indirect** visibility; sun and local-light
visibility remain separate. Direct light, light-rig emitters, emission and graphic overlays
must never be multiplied by this AO. Short-range bounce is P6, not a hidden part of P1.

**Three independent meanings of OFF:**

1. Native master OFF: execute this revision's original SSAO/shadow path, respecting the
   user's original `RenderDeferredSSAO` setting. Require byte identity with baseline.
2. Native ON with amount zero: ambient visibility is one and specular visibility is one;
   direct lighting remains unchanged. This is an identity/composition test, not permission
   to run native passes while the native master is OFF.
3. AO disabled altogether: a proposed native mode, separate from stock SSAO OFF and its
   historical shadow-blur side effect. Its shadow semantics are defined in P4.

Do not change existing saved settings merely to enter/leave native mode. A user who has
stock SSAO disabled must still be able to use native AO without triggering its listener.

## 2. Source and licensing boundary

No `.tmp/ref_private/` content or ReShade/iMMERSE shader source was opened for this plan.
In particular, the release `reshade-shaders` directory and the user's ReShade staging
directory are outside the research boundary. Existing viewer bridge code is viewer
integration code and was inspected; proprietary effect implementation is not an input.

Permitted technical sources, consulted directly:

- [Jimenez, Wu, Pesce and Jarabo, Practical Realtime Strategies for Accurate Indirect Occlusion (2016)](https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf):
  §4 defines the slice integral; §6 describes bent-normal/cone specular visibility. Use
  the mathematics independently; do not assume the paper licenses third-party code.
- [Therrien, Levesque and Gilet, Screen Space Indirect Lighting with Visibility Bitmask (2023)](https://arxiv.org/html/2301.11376v2):
  §3.1 replaces horizons with sectors, covers front/back angles and thickness, and
  explains leakage from unknown back surfaces. §3.2 motivates directional ambient
  visibility; §3.3 is the later bounce extension. The paper's temporal sampling is excluded.
- [Intel XeGTAO](https://github.com/GameTechDev/XeGTAO), especially
  [XeGTAO.h](https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.h)
  and [XeGTAO.hlsli](https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli):
  MIT source suitable for an attributed GLSL port. Its framework prepares view depth/mips,
  gathers AO/edges, and denoises spatially; bent normals are optional. It is HLSL/DX-oriented,
  not a drop-in viewer shader. Intel has discontinued maintenance.

At implementation time, pin the exact XeGTAO upstream commit and record which functions
were ported and their modifications. Preserve `Copyright (C) 2016-2021, Intel Corporation`
and `SPDX-License-Identifier: MIT` in derived files, credit Filip Strugar and Steve Mccalla,
and include the complete upstream [MIT LICENSE](https://github.com/GameTechDev/XeGTAO/blob/master/LICENSE)
in `indra/newview/licenses-win32.txt`, `indra/newview/licenses-linux.txt` and
`indra/newview/licenses-mac.txt` (all three exist). `doc/licenses/` does not exist and is
not the shipped-notice destination. The platform manifests already copy these files to
`licenses.txt` (`viewer_manifest.py:543`, `:852`, `:1121`); inspect each packaged notice in
P1 acceptance. The licence requires copyright and permission
notices in copies/substantial portions; retain the warranty disclaimer with the complete
text. Keep existing viewer LGPL notices in existing files. Do not relabel a port as wholly
independent or import unrelated sample dependencies/assets. The bitmask implementation
will be independently written from the public paper. No licence files are added in this
planning task.

## 3. Current SSAO, end to end

### 3.1 Producer, inputs and storage

| Current behavior | Evidence |
|---|---|
| `calcAmbientOcclusion` reconstructs view position from GL depth using `inv_proj`, NDC z = `2*depth-1`, and homogeneous division. Despite the local name `pos_world`, these are eye-space positions. | `class1/deferred/aoUtil.glsl:49` |
| Eight fixed 2D offsets have squared radial spacing; divide offsets by `screen_res`. A sampled 128-pixel noise pattern reflects/rotates the kernel. Radius = `min(ssao_radius / -z, ssao_max_radius)`. | `class1/deferred/aoUtil.glsl:60`, `:77` |
| Each sample contributes an inverse-distance-squared obscurance with a 0.05 normal offset and `ssao_factor_inv` cap. The depth-difference rule changes the denominator; output is clamped visibility, one = open. This is not a horizon integrator. | `class1/deferred/aoUtil.glsl:89` |
| The 128×128 RGB16F noise texture is generated with `ll_frand()` when created. It is static during its lifetime, but not a stable cross-process seed. | `pipeline.cpp:3324` |
| `hasAmbientOcclusion` attaches `aoUtil.glsl`. Shader creation chooses `sunLightSSAOF` versus `sunLightF` from the saved SSAO flag. | `indra/llrender/llshadermgr.cpp:303`; `llviewershadermgr.cpp:3066` |
| Class-2 sun SSAO output packs **R = sun visibility, G = AO visibility, B/A = two spot shadow samples**, then clamps all channels. AO is computed here, not in the later lighting compositor. | `class2/deferred/sunLightSSAOF.glsl:52` |
| `deferredLight` is allocated at the active deferred extent, with `screenFormat` RGBA16F in HDR, GL_RGBA otherwise. Screen resolution divisor/multiplier is applied before allocation; this is not an AO-specific half-resolution buffer. | `pipeline.cpp:1349`, `:1388`, `:1448` |
| Sun pass selects the active view light target, sets its dimensions and writes the fullscreen triangle. Cube captures use a separate sun-probe program with no SSAO feature. Preview captures skip the producer. | `pipeline.cpp:27310`; `llviewershadermgr.cpp:3092` |
| A light map is not guaranteed: main allocation requires shadows, stock SSAO or DOF; Prism completeness/allocation requires stock SSAO or shadows (DOF is absent from this Prism predicate). Producer dispatch separately requires stock SSAO outside cubes, shadows or lighting debug, and excludes previews. Preview quality forces the cached SSAO flag false. | `pipeline.cpp:1448`, `:1770`, `:1830`, `:27310`; `llprismpreview.cpp:1453` |
| Uniform upload scales stock radius and maximum pixel radius for snapshots, uploads factor/inverse and builds a value/saturation matrix. | `pipeline.cpp:25149` |

### 3.2 Shared blur and ShadowDist P1

`pipeline.cpp:27345` runs horizontal `deferredLight -> screen` then vertical
`screen -> deferredLight` only while SSAO is enabled, outside cube/preview captures
and raw/cascade diagnostics. It uses `RenderShadowGaussian`, `RenderShadowBlurSize`
and `RenderShadowBlurDistFactor`; snapshot autoscale multiplies the blur size
(`pipeline.cpp:27359`, `:27384`, `:27425`). The temporary `screen` content is not final beauty.

`class1/deferred/blurLightF.glsl:54` reads normal, position and light channels. It scales
offsets with normal/depth, perturbs the origin with a fixed pixel parity pattern, and
accepts neighbors by squared point-to-plane distance (`:67`). Each axis uses the center
plus six taps on either side (`:78`, `:88`, `:103`). Channels share locations and geometry
rejection, but `.xyxx` weights give green its own kernel weight. Thus stock AO toggling
also changes sun softening; it is not just an AO quality switch.

ShadowDist P1 changes **red only after the old blur** (`class1/deferred/blurLightF.glsl:121`):

- 0: stock shared kernel;
- 1: dedicated Gaussian sigma in pixels;
- 2: Gaussian sigma in world metres, projected using depth and pixels/metre;
- 3: preserve center red, no sun blur. Green still receives stock blur in every mode.

CPU clamps mode to 0..3, scales pixel sigma for snapshots, converts millimetres to metres,
and derives pixels/metre from the bound height and live projection (`pipeline.cpp:27389`).
The shader caps its sampling radius/budget and uses integer-grid red taps
(`class1/deferred/blurLightF.glsl:128`). These controls are runtime uniforms, not shader reload switches.
Their defaults are mode 0, 1 px and 2.5 mm (`app_settings/settings.xml:20817`).
They remain ineffective when the SSAO blur dispatch gate is false. P4 must preserve
their useful sun-quality choices even if it retires the coupling workaround.

`RenderDeferredBlurLight` is declared at `app_settings/settings.xml:20700`, but the audited C++ has
no matching dispatch read/listener; do not mistake it for a functioning decouple switch.
Actual blur controls are cached/listened at `pipeline.cpp:931`, `:944` and read at `:2889`,
`:2902`.

### 3.3 Composition: what gets darkened today

`class3/deferred/softenLightF.glsl:180` reads light-map R/G separately. `scol` is red,
`ambocc` is green. `adjustIrradiance` (`:137`) mixes the original irradiance with an
occluded endpoint: value/saturation transform of irradiance × scale, limited by an
absolute ceiling. Ambient tint also colors the ceiling. This is not simply `irradiance *= AO`.

| Path | Current composition and consequence |
|---|---|
| Deferred PBR | `class3/deferred/softenLightF.glsl:222`: ORM red is **material-authored AO**; green/blue are roughness/metalness. Probe irradiance and radiance are sampled at `:234`; only irradiance goes through SSAO at `:236`, followed by Shadow Lift. `pbrBaseLightKeep` at `:244` evaluates IBL and sun separately. Screen SSAO does not currently attenuate probe radiance. |
| PBR IBL | `class1/deferred/deferredUtil.glsl:718`: authored AO multiplies diffuse IBL and drives `computeSpecularAO` for specular IBL. It is distinct from screen visibility. `:1742` separates IBL, direct sun and emission. Preserve that separation. |
| Deferred legacy/Blinn-Phong | `class3/deferred/softenLightF.glsl:272`: probe ambient goes through `adjustIrradiance` at `:286`. Sun is added separately (`:294`); gloss/environment reflections are applied later (`:341`, `:351`) without screen AO. Base-color alpha fullbright mixing (`:346`) must retain its semantics. |
| Classic skies | Legacy sums ambient and sun in sRGB before converting to linear (`class3/deferred/softenLightF.glsl:299`); PBR also recombines nonlinearly (`class1/deferred/deferredUtil.glsl:1763`). Gate the unchanged direct input before this sum. The legacy `debug_direct` subtraction at `softenLightF.glsl:309` depends on ambient and is not the oracle. Changing ambient can change the apparent final sun difference; stock SSAO behaves the same. Exact final-image direct equality is impossible in this compatibility path. |
| Forward legacy alpha | `class2/deferred/alphaF.glsl:435`: probes/ambient, Shadow Lift and direct sun are evaluated, with an explicit **no SSAO** comment at `:439`. Local lights are separate (`:373`). |
| Forward legacy material BLEND | `class3/deferred/materialF.glsl:568` has its own lit forward branch: probe ambient at `:598` and explicitly no SSAO at `:599`, separate sun at `:603`, gloss/environment at `:646`/`:657`, local lights at `:692`. Non-BLEND writes the deferred G-buffer (`:768`). Include this additional alpha path in any native receiver/probe changes; changing only `alphaF` misses legacy materials. |
| Forward PBR alpha | `class2/deferred/pbralphaF.glsl:369`: probes then Shadow Lift, explicitly no SSAO; `:384` calls PBR base lighting with material AO. Local lights use the forward helper (`:332`). |
| Shared Actor FX PBR replay | `class2/deferred/sharedActorFxPbrF.glsl:597`: OPAQUE/MASK use opaque probe policy, BLEND uses transparent policy; all currently bypass screen SSAO (`:610`). PBR beauty, authored emission and creative treatment have distinct ownership (`:621`, `:632`). An OPAQUE replay can therefore differ from the deferred original unless native AO is integrated here too. |

The key advantage over final-image AO already exists in the viewer's irradiance path.
Improve its visibility and add probe-only specular occlusion; do not introduce a beauty
multiply. Stock indirect adjustment should initially remain for an interpretable A/B;
a later native physical multiply/look response must be explicitly selected, not silently
stacked with `adjustIrradiance`.

### 3.4 Complete SSAO setting and UI inventory

These are the six actual `RenderSSAO*` settings plus the deferred master. All line
references in the declaration column are `app_settings/settings.xml`.

| Setting | Type/default; declaration | Live plumbing / persistence |
|---|---|---|
| `RenderDeferredSSAO` | bool true; `:20688` | Persist 1. Cache signal `pipeline.cpp:896`, read `:2854`; **shader reload listener** `llviewercontrol.cpp:1237`. Handler refreshes caches and calls `setShaders()` (`:216`). Changes producer selection and `HAS_SSAO` compositor permutation (`llviewershadermgr.cpp:3066`, `:3661`). |
| `RenderSSAOScale` | F32 500; `:18778` | Persist 1. Runtime cache signal `pipeline.cpp:932`, read `:2890`, radius upload `:25155`. Pixels at 1 metre, not a physical radius in metres. |
| `RenderSSAOMaxScale` | U32 200; `:18811` | Persist 1. Signal `:933`, read `:2891`, pixel-cap upload `:25156`. |
| `RenderSSAOFactor` | F32 0.30; `:18822` | Persist 1. Signal `:934`, read `:2892`, factor/inverse upload `:25158`. |
| `RenderSSAOEffect` | Vector3 (0.8, 1, 0); `:18833` | Persist 1. Value/saturation, unused third component. Signal `:935`, read `:2893`, matrix upload `:25162`. |
| `RenderSSAOIrradianceScale` | F32 0.6; `:18789` | Persist 0. `LLCachedControl` in lighting pass `pipeline.cpp:27709`, uploaded every bind. No shader reload listener found. |
| `RenderSSAOIrradianceMax` | F32 0.18; `:18800` | Persist 0. Same live path; ceiling additionally scaled with EnvIntensity probe-diffuse factor at `pipeline.cpp:27725`; tint occurs in shader. |

Only the master has the SSAO shader-reload listener; scalar/vector controls refresh
cached state or are cached directly. Reserved uniform enum/string order is
`indra/llrender/llshadermgr.h:186` / `indra/llrender/llshadermgr.cpp:1479`.

UI/preset surfaces:

- Preferences Advanced has `UseSSAO`, bound to the master
  (`skins/default/xui/en/floater_preferences_graphics_advanced.xml:793`). Capability
  gating is in `llfloaterpreferencesgraphicsadvanced.cpp:393`, `:427`, `:465`.
  Feature tables enable it for higher graphics tiers and can disable availability
  (`featuretable.txt:65`, `:253`, `:404`, with Linux/Mac counterparts).
- Director World > Shadows has the advanced SSAO toggle and the explicit warning about
  shadow smoothing (`skins/default/xui/en/panel_dpage_world_shadows_probes.xml:20`).
  Softening modes are at `:37`, pixel/mm sliders at `:52`/`:57`, shared blur size at
  `:199`, Lightbox link at `:209`. Search entries are in
  `skins/default/xui/en/director_kit_index.xml:2135` and `:2138`.
- Lightbox SSAO controls occupy `skins/default/xui/en/floater_lightbox_settings.xml:2891` through the scale
  controls at `:3160`; indexed at `skins/default/xui/en/director_kit_index.xml:2370`. Its
  `RenderSSAOEffectValue` / `RenderSSAOEffectSat` bindings (`skins/default/xui/en/floater_lightbox_settings.xml:2916`,
  `:2973`) **do not have matching declarations or C++ adapters in this audited tree**.
  Treat these as unresolved legacy UI bindings, not additional working settings.
  P5 must bind native appearance values to declared controls and test them end to end.
- Phototools links to Lightbox for full SSAO control
  (`skins/default/xui/en/floater_phototools.xml:353`); it is not a separate native AO panel.
- Graphics presets explicitly include the old master (`llpresetsmanager.cpp:315`, `:324`).
  Native quality needs deliberate inclusion; a new setting is not saved automatically
  everywhere just because `Persist=1`.

## 4. G-buffer and view inputs

| Input | Available representation; integration rule |
|---|---|
| Depth | `deferredScreen.allocate(..., true)` at `pipeline.cpp:1385`; default `DEPTH_FMT_24` in `indra/llrender/llrendertarget.h:92`, allocated as `GL_DEPTH_COMPONENT24` by `indra/llrender/llrendertarget.cpp:130`, `:430`. Ordinary GL depth, clear/far = 1, not reverse-Z. Shared with scene screen at `pipeline.cpp:1443`. |
| Reconstruction | `class1/deferred/deferredUtil.glsl:656` reconstructs with `inv_proj`, NDC `2*d-1`. Linear distance is `-view.z`. Use active view projection including asymmetric tile/lens projections; do not copy DX depth constants/signs into GL. Test near/far, off-axis, divisor and odd extents. |
| Base color | Attachment 0 `GL_SRGB8_ALPHA8` (`pipeline.cpp:1385`); legacy alpha carries lighting/fullbright semantics. Never repurpose it for AO. |
| Materials | Attachment 1 GL_RGBA (`pipeline.cpp:689`): legacy specular/gloss or PBR ORM. `class1/deferred/gbufferUtil.glsl:36` decodes the layout; `class1/deferred/pbropaqueF.glsl:146`, `:213` writes ORM. No per-object UUID/material-thickness ID exists in this G-buffer. |
| Normals | Attachment 2 is **GL_RGBA16 normalized**, not RGBA16F, in HDR; non-HDR `GL_RGB10_A2` (`pipeline.cpp:689`). Octahedral **view-space shading normal** in XY, environment/AvatarLightResponse carrier in B, flags in A (`class1/deferred/globalF.glsl:55`; `class1/deferred/gbufferUtil.glsl:52`). `decodeNormal` does not initialize W (`class1/deferred/globalF.glsl:80`); fetch raw flags separately. |
| Geometric normal | Not stored separately. PBR normal maps are applied through TBN before writing (`class1/deferred/pbropaqueF.glsl:135`, `:144`), with front/back flip at `:190`. Reconstruct an edge-aware macro normal from depth for bias/validity; use shading normal for the lighting hemisphere, with a bounded correction policy. Do not interpret a shading normal map as physical occluder relief. |
| Flags / actor identity | Flag values and optional tag decoding are injected at `indra/llrender/llshadermgr.cpp:697`, `:718`. Avatar tag is HDR-dependent (`class1/deferred/globalF.glsl:63`); not a universal actor mask or material ID. Use helpers, never hard-coded comparisons to packed alpha. |
| Emissive | Optional attachment 3, RGBA16F HDR / GL_RGBA otherwise (`pipeline.cpp:689`). Alpha can carry silhouette metadata. Do not steal it for bent normals. |
| Velocity | `mVelocityMap` RG16F, separate geometry pass sharing opaque depth (`pipeline.cpp:1585`, `:8483`; `pipeline.h:1698`). It is not an always-valid MRT, not owned by every feed. AO must not allocate or sample it; no history/reprojection. |
| Half resolution | No reusable half-res AO buffer exists. Relight has per-pack `areaGather/areaScratch/areaVisibility` (`pipeline.h:1361`) and ceil-half extent logic (`pipeline.cpp:26960`). Projector shafts have separate half/history targets (`pipeline.h:1591`, `:1612`). Reuse allocation/math patterns, **never alias live targets or temporal histories**. |

Opaque and alpha-tested fences/foliage appear in deferred depth/normals. Alpha mask
discard and normal write can be seen in `class1/deferred/diffuseAlphaMaskF.glsl:42`.
Blended hair/lace do not become full opaque occluders by virtue of appearing in beauty.
An AO bitmask fixes height-field over-occlusion where depth exists; it cannot recover
missing transparent layers, backfaces or offscreen geometry.

## 5. Ambient, reflection probes and double-counting

Current class-3 probe sampling separates diffuse and glossy directions:
`class3/deferred/reflectionProbeF.glsl:763` samples ambient with the normal, radiance
with reflection direction and roughness LOD, then mixes SSR (`:783`) and hero probes
(`:802`). Legacy has separate gloss/environment outputs (`:879`) and the same replacement
ordering (`:911`). Probe diffuse includes `sky_gi_scale`/ambient tint and ambiance mixing
(`:563`); specular EV is applied before SSR/hero replacement (`:778`). Class-2 fallback
uses ambient blending and an environment cube (`class2/deferred/reflectionProbeF.glsl:46`,
`:77`); test it explicitly when real reflection probes are disabled.

Do not conflate these existing mechanisms with native local AO:

- Authored ORM AO and its `computeSpecularAO` response are material-scale data
  (`class1/deferred/deferredUtil.glsl:736`). They remain active exactly once.
- Probe `refParams` contain ambiance, radiance scale, fade and near-z
  (`llreflectionmapmanager.cpp:3362`). The `.z` weight is fade-in
  (`class3/deferred/reflectionProbeF.glsl:495`), **not a hidden screen-AO term**.
- `LLReflectionMap::doOcclusion` is a bounding-volume GPU occlusion-culling query
  (`llreflectionmap.cpp:316`), not per-receiver specular visibility. Do not disable it.
- Captured probe light already contains scene-scale visibility. Additional AO must be
  short-range; screen AO stays excluded from probe baking to prevent feedback/double bake.

P3 hook design:

1. Carry **shading N, diffuse sampling bent N and visibility-cone data separately**.
   Never pass bent N as the one normal argument to the existing probe routine: that
   would also bend the specular reflection vector and modify SSR geometry.
2. Native probe overload samples irradiance using bent N, keeping existing influence,
   tint, ambiance and EV logic. Apply scalar native visibility once on this irradiance.
   Select either stock-compatible response or native response, not both.
3. Calculate a bounded roughness/view/cone specular factor from bent direction + open
   visibility. Multiply **probe/environment radiance before SSR/hero replacement**.
   Leave punctual BRDF normals, sun visibility and local-light data untouched. Hero
   mirrors/SSR should initially retain their current policy; extending occlusion to
   them requires separate evidence, not multiplying the already mixed radiance.
4. Preserve authored material diffuse/specular AO. Native specular visibility applies
   once independently; do not also feed `materialAO * nativeAO` into
   `computeSpecularAO`, which would count native AO twice. Correlated baked cavities
   can still over-darken; compare authored AO=1 versus cavity maps and keep native radius
   short. A combined-visibility heuristic would be an explicit later look decision.
5. With visibility=1, bent direction must equal shading N and native specular factor=1.
   With near-zero open moment, use N and zero/bounded visibility, never normalize zero.
   A single bent cone cannot represent multiple separate openings perfectly; do not
   promise the complete directional lighting result of a stored per-sector distribution.

**Separate legacy-path design:** `sampleReflectionProbesLegacy` returns `glossenv` and
`legacyenv` after SSR/hero mixing; they are later applied to lit color at
`softenLightF.glsl:341` and `:351`. Do not multiply these returned mixed values by AO.
The native overload applies the cone factor independently to the original linear
gloss probe sample after its EV scale (`class3/deferred/reflectionProbeF.glsl:895`),
and to the original linear environment sample after its EV scale (`:904`), before
the two SSR mixes (`:926`, `:927`) and hero replacements (`:933`, `:934`). Preserve
the shading-normal reflection vector and the original replacement weights. Bent N
changes only the ambient sample. Retain separate factors for gloss LOD and the
legacy sharp environment lookup; specular visibility is not diffuse AO copied twice.

Keep `applyGlossEnv`'s Fresnel, specular color and existing color-dependent energy
heuristic (`:940`) and `applyLegacyEnvKeep`'s authored `envIntensity` mixing weight
(`:954`) unchanged. The class-2 fallback stores legacy environment in sRGB
(`class2/deferred/reflectionProbeF.glsl:92`): in its native overload decode the probe
sample, attenuate in linear space, then re-encode before the original legacy mix.
Factor=1 bypasses the new conversion for exact identity. Cover deferred legacy,
forward legacy alpha/material receivers if enabled, and class-2 probes-disabled skies.
P3 must isolate probe-only tests with ambient held equal: the legacy color-dependent
heuristic and nonlinear environment mix mean a final-beauty subtraction cannot prove
that an SSR/direct input changed. Direct BRDF inputs and SSR/hero replacement inputs
must be unchanged independently.

Shadow Lift currently follows SSAO (`pipeline.cpp:27709`; `class1/deferred/deferredUtil.glsl:1728`). Keep
that order initially, including EnvIntensity ceiling scaling. Do not darken the whole
post-Shadow-Lift beauty or rig rim to compensate for an exposure mismatch.

## 6. Relight reuse and thickness

The Relight brief is historical design; this checkout **already implements** area
shadow gathering/filtering. Follow actual source and the Step 6 audit, not its original
"not built" statement.

Reusable concepts:

- `class1/deferred/screenSpaceRayUtil.glsl:10` snaps to the full-depth texel actually read,
  including half-res/odd-extent ties; `:24` gives current-view linear depth; `:33` computes
  receiver footprint at receiver depth. Port/generalize these concepts for AO without
  pulling in unnecessary light-specific uniforms.
- Its tracer validates finite penetration, source exclusion and a refined surface hit
  (`:44`, `:69`). AO is a slice gather, not a per-light trace; do not run this entire
  tracer for every sector.
- `class3/deferred/areaShadowFilterF.glsl:12` has depth/normal bilateral rejection;
  `:44` upsamples using actual full-res sample positions; `:61` handles zero weights.
  Useful validation patterns, but XeGTAO's denoiser and edges need their own implementation.
- `alareashadowmath.h:16` centralizes view policy; `:65` defines quality; `:78` starts a
  fixed-work plan, reducing work deterministically; `:118` counts actual feed passes;
  `:134` reserves main work before assigning feed allowances. Use an AO-specific pure
  policy/math header instead of coupling AO quality to area-light channel count.
- `pipeline.cpp:26987` polls asynchronous timestamps without blocking. Copy its
  nonblocking principle, not the main-view timer/query ownership.

**Thickness must be separate.** `RenderAreaShadowThickness` defaults to 0.35 m
(`pipeline.cpp:26784`); in the ray helper it tolerates a front-depth crossing plus step
length and validates a plane hit. Bitmask thickness models the **back extent of each
sample** and determines an occupied angular interval. Same units do not imply the same
meaning. Reusing 35 cm would make fingers, railings and hair cards behave like thick slabs.
Sharing a setting would also let an AO look unexpectedly alter light shadows.

Propose `RenderNativeAOThicknessM = 0.03 m` as a **calibration starting point**, range
0.001..0.5 m, finite/clamped, with a separate receiver bias. Test 0.01/0.03/0.10 m across
SL avatar and architectural scales before selecting a shipping default. Start constant
in metres; distance-scaled thickness is an optional separately named parameter, default
zero, because it changes the look as the camera moves. A single front depth cannot
identify the true thickness of an arbitrary mesh.

## 7. Fork interaction policy

| Feature | Current hook / evidence | Native AO policy |
|---|---|---|
| Cine Light Rig and Rig Rim | Rig/ActorMask lighting dispatch in `pipeline.cpp:27818`, `:28127`; rim data/helpers `class1/deferred/deferredUtil.glsl:1064`; `alcinelightrigmanager.cpp:879` identifies projector/bounce emitters for rim. `alcinerigrim.cpp:149` gates rim in HUD, probe and alpha passes. | Do not attenuate direct key/fill/rim/background, their synthetic bounce lights, or rim overlays with AO. Ambient/probe illumination on their receivers can change. A light called "bounce" is still evaluated as a local light here. `CineRigRimIncludeProbes` controls whether rim is rendered into probe captures; it is not an extra probe-lighting input. Keep those captures AO-free. Their captured rim contribution can subsequently receive ordinary indirect probe occlusion at a receiver. |
| Actor Mask | Per-view classification/validity in `pipeline.cpp:27194`, `:27234`, actor-only receiver routing at `:28127`; separate depth-sharing mask lifecycle at `:1371`, `:1444`. | Default world AO applies to opaque avatars as ordinary receivers/occluders. The light-recipient mask is not a blocker-depth mask. A future actor exemption can return visibility=1 for recipients, while retaining their occluder depth. Never remove world depth based on actor bits. HDR tag alone is insufficient for animesh/custom targets. |
| Actor FX | Deferred PBR changes material normal/ORM/emission (`class1/deferred/pbropaqueF.glsl:137`, `:164`, `:192`); shared replay path `class2/deferred/sharedActorFxPbrF.glsl:597`. | Physical shaded surfaces receive indirect AO. Cover/sensor/flat/emissive treatments retain their authored overrides; do not AO-multiply creative post-light color or emission. Add OPAQUE/MASK replay parity in P1/P3; BLEND follows the alpha policy below. |
| Tron | Scene-layer dispatch and depth/state rules in `pipeline.cpp:8854`, `:8886`, `:8914`; avatar flag encoding `class1/deferred/globalF.glsl:63`. | AO may change the physical surface behind Tron. Do not attenuate neon/emissive grid, trails, rim tint/pulse, or screen-space graphics. Keep raw normal flags/sidecars intact. |
| Roto Ink / silhouette | Outline Scene layer at `pipeline.cpp:8834`, `:8919`; other layer handling `:15187`; silhouette carrier decode `class3/deferred/softenLightF.glsl:168`. | Do not alter outline extraction inputs or post outline colors. Shade physical fill before its creative overlay. Preserve silhouette K/R, fullbright and emissive policy; zero-emission and flat-fill fixtures must retain their promised treatment. |
| VCam / Prism camera feeds | Exact-size pool at `pipeline.cpp:1746`; per-view save/invalidate at `:11479`, restore at `:11668`. Relight Step 6 documentation `doc/director_kit/RELIGHT_SL_INTEGRATION.md:788`. | P1/P2 main only, status says feeds use their existing AO policy. P3 integration substep adds native feed opt-in with pack-local AO resources, view state and fixed allowances. Never sample main AO in a feed. Pool storage is per size bucket, reused sequentially, not permanently one allocation per camera ID. |
| Preview atlas / media / Gate | Preview quality forces SSAO/shadows off and restores them (`llprismpreview.cpp:1443`); raw G is neutralized in `pipeline.cpp:27293`. | Preview remains AO-neutral. Media has no new depth. Gate camera sources run AO only when actually rendered in an eligible scratch view, not again at display composition. |
| HUD | Deferred lighting asserts `!sRenderingHUDs` (`pipeline.cpp:27246`); Relight policy rejects HUD (`alareashadowmath.h:16`). | Explicitly reject `sRenderingHUDs` at AO allocation admission, dispatch and consumer validity. HUD gets visibility 1 and cannot consume retained world AO even if it shares a target. |
| Hero-mirror pass / cube captures | Hero swaps to `mHeroProbeRT` (`llheroprobemanager.cpp:309`); mirror identity is `isMirrorPass()` (`llheroprobemanager.h:88`); Actor Mask separately recognizes it (`pipeline.cpp:27199`). | Reject `gCubeSnapshot`, hero pack identity and `isMirrorPass()` independently; hero mirrors are not opted-in VCam feeds. No native AO/bent sampling in captures, no feedback baking. Retain the stock capture sun path. Main-view hero replacement inputs stay unoccluded by probe-only AO. |
| Impostors | `generateImpostor` sets `sImpostorRender` (`pipeline.cpp:32753`) and renders into avatar-owned targets (`:32900`). | Reject generation/update passes; no native allocation, gather, publication or consumer lookup. An impostor drawn into the ordinary world may receive the world depth-based policy, with its billboard-depth limitation disclosed; do not bake native AO into its cached image. |
| Water reflection renders | `sReflectionRender` exists at `pipeline.cpp:658`; it is also used by forward impostors (`:32750`). Relight rejects reflection views (`alareashadowmath.h:16`). | Reject reflection flag and any water reflection camera/target route, never infer main view from a false flag alone. This checkout has no separate active water-reflection flag setter beyond the impostor route; use a synthetic reflection-view dispatch fixture as well as real available routes. Ordinary main-view water follows BLEND policy; reflection captures never reuse main AO. |
| DOF scratch reuse | CoF overwrites `deferredLight` at `pipeline.cpp:23727`; DOF samples it at `:23780`, `:23802`. | Finish all light-map consumers before CoF; invalidate published-green lifetime before that overwrite. Keep `aoResolved` separate and valid only for its owning lighting view. Reinitialize R/G/B/A on every later native lighting dispatch; DOF scratch must never be mistaken for last-frame visibility. Do not allocate/rebuild the screen pack for an AO toggle. |
| High-resolution stills | Snapshot autoscale set/reset `llfloatersnapshot.cpp:778`, `:1343`; projection/pixel kernel handling `pipeline.cpp:27364`, `:27407`. | P1 support single-target high-res renders with actual projection and size. Physical AO radius/thickness are not multiplied by snapshot autoscale; only a reference-pixel cap/filter width uses full output dimensions if needed. Test 1×/2×/4× framing. |
| Tiled snapshots | `llviewerwindow.cpp:6003`, `:6143` loops subimages through `display`; Relight explicitly rejects zoomed tiles (`pipeline.cpp:26619`). | Initially fall back to stock for the **entire tiled shot**, with explicit status. Later use padded tiles with overlap ≥ projected AO reach + all filter support, crop interiors, use global output-pixel noise coordinates. No per-tile unpadded native AO: neighboring off-tile occluders are absent. |
| ReShade bridge | Depth and preserved normal/albedo sidecars published in `llreshadebridge.cpp:355`; preserve-copy reason `:376`. ABI 1.2 fields/encoding at `llreshadebridgeabi.h:76`, `:162`, `:179`; generation tracking `llreshadebridge.cpp:469`. | Preserve sidecars and layout. Recommend native AO with external MXAO/RTGI AO disabled by the user to avoid stacking. Native AO export is optional later ABI work; do not overwrite ORM red or encoded normal channels. No automatic manipulation of proprietary effects/settings. |

### Alpha policy: coverage is a real limitation

P1 must include deferred OPAQUE/MASK and matching shared Actor FX replay consumers. A
MASK hole stays a hole; test both sides of two-sided cards. Blended alpha remains on
its current unoccluded-indirect path initially, explicitly disclosed in status/help.
Do not advertise correct AO casting from blended hair just because bitmask AO is enabled.

P2 includes a bounded experiment for alpha **receivers**: only reuse resolved visibility
when fragment depth/normal match the opaque receiver within a footprint-scaled tolerance;
otherwise use visibility=1. That prevents wall AO from painting dark patches onto
foreground hair. This conservative method will reject many hair pixels and is not full
alpha support. If acceptable, apply it to `alphaF`, `pbralphaF` and shared BLEND alike,
before direct/emissive contributions, including the legacy material BLEND branch in
`class3/deferred/materialF.glsl:568`. Make this an independent opt-in until parity passes.
True alpha occlusion needs a coverage/depth representation (possibly another layer);
defer that separately with its bandwidth and transparency policy rather than silently
changing depth writes, alpha cutoff or blending.

## 8. Proposed native passes and resources

Use fullscreen raster GLSL passes first, fitting existing render-target/shader binding
patterns and avoiding an additional compute-only hardware requirement. XeGTAO's math
and denoiser can be MIT-ported; its DX UAV dispatch and coordinate conventions cannot.
Keep uint bit operations at the supported GLSL level, with a bounded-loop fallback for
population count if required by the viewer's shader version. A compute backend is later
optimization, requiring identical output gates.

Order: deferred geometry -> existing sun/shadow producer and its required blur -> native
depth prepare -> slice gather -> spatial filter/resolve -> green-channel publication ->
ambient/sun compositor -> local lights -> forward/replay consumers -> existing post.
Depth prep may execute earlier after geometry if dependencies permit; do not move beauty
or bridge-preservation ownership accidentally.

| Resource / pass | Proposed format, size and contract |
|---|---|
| `aoDepth` prepare | R32F linear **positive -view.z**, full extent, five levels initially. Write per-mip explicitly using depth-aware reduction; do not call generic averaged `glGenerateMipmap` for geometry depth. Background sentinel is explicit and never a real blocker. Use base-level depth for finest samples/edge tests. |
| Gather P1 | Inputs active depth/mips, shading normal and depth-derived macro normal, projection/extent, radius/bias, pixel-stable noise. XeGTAO-style slices and paired radial samples produce visibility in [0,1] plus packed edge confidence. |
| Gather P2 | Same footprint; per slice a register-local 32-bit occupancy mask of the normal-centered angular hemisphere. Finite front/back interval comes from sample position and assumed thickness behind it along the camera ray. Clip intervals, convert cosine to angular space, classify sector overlap and OR occupancy. Do not reinterpret cosine directly as uniformly spaced angle. |
| `aoRaw`, `aoScratch` | R16F each for scalar AO at full or ceil-half dimensions. Edge metadata R8 at gather extent. Denoise raw -> scratch -> raw -> scratch at minimum (three full spatial draws without TAA); resolve reads the final ping-pong target. Never sample and render the same image. Retain bilateral/edge validity at each step. |
| `aoResolved` | R16F full extent. Joint depth/normal upsample when gather is half resolution. At an edge with no valid source, return neutral visibility instead of spreading foreground occlusion across the background. An optional full-res refinement at depth edges must be separately budgeted. |
| Bent variant P3 | Replace raw/scratch/resolved scalar formats with RGBA16F: XYZ unnormalized open-direction moment in view space, A scalar visibility. Filter moments, normalize after resolve; do not blur oct-encoded XY directly. Full-resolution resolved result is consumed by native probe shaders. Edge texture remains R8. |
| Publish | Green-only draw into the active `deferredLight`, sampling the separate resolved target. Save/restore indexed/global color masks and all other GL state. R/B/A are not read-modify-written or rounded again. This preserves the established AO slot without a new G-buffer layout. |
| Consumer P1/P2 | Existing `adjustIrradiance` behavior, one native green producer replacing stock green. Precompiled native compositor handles configurations where stock `HAS_SSAO` is absent. Original compositor is selected whenever native is OFF. |
| Consumer P3 | Separate native probe entry points and precompiled native lighting programs. Material normal, authored AO and direct paths retain original roles. Never attach bent data to existing material/bridge channels. |

Use the receiver's view-ray direction for the assumed back extent, with GL eye-space
signs verified against a constructed slab; a fixed camera -Z offset is not equivalent
at the image edges. Thickness is a reconstruction assumption, not actual mesh volume.

Depth mip safety is important for the bitmask: a coarse filtered depth cannot represent
all narrow gaps in its footprint. Use conservative sample-LOD limits near discontinuities,
nearest source association and fine-level fallback/refinement. Validate thin fixtures at
each LOD; XeGTAO horizon heuristics cannot be assumed correct for finite interval masks.
P2 should first pass with LOD0, then re-enable mip sampling behind those gates.

Integrate slice contributions with the appropriate projected-normal/cosine weights.
Use analytic angular-sector weights (or a separately verified approximation), not an
unexamined `1 - bitCount(mask)/32` transplanted into the GTAO integral. A CPU numerical
hemisphere oracle establishes the convention: one means unoccluded. Compute bent moments
from **open** sectors using the same measure. Do not reuse the closed-form two-horizon
bent-normal formula after switching to a disconnected mask.

All shifts must be unsigned and defined for empty/full intervals: handle a 32-sector
span explicitly rather than shifting by 32. Clamp/validate inputs, acos arguments,
projection division, radius and normals. Empty/sky/outside-view samples are unknown/open,
not black borders or wraparound blockers. Missing information can leak; it must not NaN.

### Lifetime, runtime selection and failure policy

**P1-A decision: option (b), reserve `deferredLight` only after explicit preparation.**
Separate static support, preparation intent, admission and active execution:

- `nativeStaticSupported(extent)` uses only the required GL version/extensions,
  texture/render-target format support, limits and positive supported extent. No live
  free-VRAM value, texture discard level, timing, governor state or master setting is
  an input. Require the viewer's raster GLSL path with defined 32-bit uint operations
  (bounded-loop sector counting may replace missing popcount), sampled GL depth,
  renderable R32F/R16F/R8 and the current light-map format; RGBA16F is additionally
  required for a bent plan. Each extent/mip must fit static texture/renderbuffer limits
  and checked integer dimensions. Compile/link success is a separate preparation gate.
  Pack/view eligibility is the explicit policy in §7: main, plus opt-in real
  feed packs after P3; never preview, aux/cube, hero, HUD, impostor or reflection views.
- Proposed `RenderNativeAOPrepare` (**Prepare native AO**) is machine-level, default
  **false**; exclude it from scenes, creative presets and Quick. Define reservation
  capability as `nativeStaticSupported && RenderNativeAOPrepare && eligiblePack`.
  Capability alone is not allocation success, memory admission or permission to draw.
  `nativeReserveRequestedForPack` is a latched reservation intent: propose it only in
  that explicit preparation transaction, commit it with the prepared generation, and
  retain it until explicit teardown/reprepare. Failed admission leaves the prior latch
  unchanged; it is never recomputed from current free VRAM in allocation/completeness.
- A requested plan passes the separate §9 preflight once at explicit preparation.
  Success latches `nativePreparedForPack` with extent/format/plan generation and owns
  its resident resources. Failure leaves preparation unavailable, stock running, and
  no partial reservation. No automatic retry or capability change follows VRAM pressure.
- Extend main allocation `pipeline.cpp:1448` with the explicit reservation request
  (`shadow_detail > 0 || ssao || RenderDepthOfField || nativeReserveRequestedForPack`);
  extend Prism completeness `:1770` and allocation `:1830` identically. Evaluate changed
  reservation requests only at the controlled preparation/teardown or ordinary extent
  lifecycle boundary, never from a per-frame memory predicate. Keep DOF out of the feed
  predicate; preview's pinned map at `:1863` retains its existing lifetime.

**Exact preparation sequence, before a take:**

1. With Prepare OFF, use stock allocation and startup programs only: zero native bundle,
   additional light-map reservation, native program links or native cache allocations.
   Ordinary stock shadows/SSAO/DOF costs remain the baseline. A native-master request
   while unprepared reports "Prepare native AO before enabling" and stays on stock.
2. User turns Prepare ON outside capture. Freeze the requested eligible extents/formats,
   algorithms and scalar/bent modes; check static support, then run combined memory/work
   admission including existing allocations, streamed-texture headroom and transition peak.
   Display the additional resident bytes and any reduced plan before marking it ready.
3. Link/load native companions during this explicit prepare step (or startup if Prepare
   was previously saved ON); never reload stock shaders through `setShaders`. Allocate
   each missing/changed native target and required light map **once**, reuse existing
   stock maps, initialize contents/validity as below, then atomically publish Prepared.
   Prefer a targeted pack preparation helper; never call `allocateScreenBuffer` merely
   to add the map. Roll back new resources on failure, leaving the old valid path intact.
4. Pin the admitted plan and capture-governor snapshot before a take. Native execution
   requires `Prepare && nativePreparedForPack && matchingGeneration && eligibleView &&
   RenderNativeAOEnabled` plus valid current-view inputs/programs. Prepared OFF/ON toggles
   select existing programs/storage only: zero allocation/release, shader reload/link,
   screen-pack rebuild or legacy-setting writes. Toggle OFF retains prepared resources.
5. Prepare OFF is an explicit teardown outside a take: first select stock, invalidate
   native publication, release native-only storage/programs and the reservation-only map
   when no stock/DOF owner needs it. During a take, preparation, teardown and changed
   extent/format/quality requests stay pending until capture ends. GL loss aborts the
   take with fallback; restoration requires preparation again. Pressure alone never
   flips the allocation gate or silently rebuilds a pack mid-session.

This retains the established R/G/B/A contract, existing bind/unbind consumers and DOF's
scratch target, with less parallel routing than option (a), a native-owned resolved
RGBA light map. Option (a) would need independent sun-channel initialization, blur,
all-consumer rebinding and DOF lifetime isolation; it also duplicates a full-size RGBA
target when stock shadows already allocate one. Option (b)'s explicit cost when the
old gate was false is 8N bytes HDR (15.8 MiB at 1080p / 63.3 MiB at 4K), nominally 4N
non-HDR; account the actual allocated format. Add the AO bundle (scalar approximately
17–98 MiB across the listed 1080p/4K modes), not just this map. These bytes share texture
accounting and can reduce streamed-texture quality (`llviewertexture.cpp:654`, `:673`).
Only users who explicitly enable Prepare pay this retained OFF cost; stock users with
Prepare OFF pay no native overhead. Prepared OFF still requires byte-identical rendering
and zero native execution.

**P1-B: reserved storage is not valid lighting content.** `bindDeferredShader`
(`pipeline.cpp:25082`) substitutes white only for an incomplete target; a complete
unwritten reserve otherwise binds uninitialized data. Clear a newly allocated native-
reserved light map to **RGBA=(1,1,1,1)** once, with framebuffer/state restoration, before
exposing it. Also track pack-local content ownership/generation: Neutral, current-view
Lighting, or DOF scratch. Allocation completeness never implies Lighting validity.

For OFF, preserve the old producer gate. If it would not have written this view's map,
bind the original white fallback in **every lighting consumer**, including soften's
debug path, even if reserved storage is complete or retains prior native/DOF contents.
If the old gate runs, allow its completed current-view stock lighting map normally.
Audit explicit light-target bindings as well as `bindDeferredShader`; no native OFF
uniform/pass is needed to select this CPU-side binding. DOF's `dofCombineF` binding is
separate: accept only its current post-lighting scratch generation after CoF/required
beauty writes have completed, preserving the stock path, never a neutral or stale AO map.
At each view begin invalidate lighting ownership; after CoF overwrite mark DOF scratch.
White allocation clear plus these ownership gates cover first OFF frame and ON->OFF
after DOF without clearing the reserved map every frame.

Repair dispatch independently of allocation: at `pipeline.cpp:27310`, preserve the
entire old predicate for stock/OFF, but OR in `nativeActiveEligibleView` under
`!sPrismPreviewRender`. Native activation requires `!gCubeSnapshot` plus the complete
view policy above; merely having a complete light map never makes a view eligible.
For native ON with stock SSAO=false, shadows=0, DOF=false and debug=0, clear all four
channels to 1, run the precompiled no-stock-AO sun producer (R/B/A remain neutral), then
publish native G. When shadows/debug need real channels, run their proper producer
before green publication. Preserve preview's white clear and `llprismpreview.cpp:1453`
override; do not let the new OR bypass it. Cube captures retain only their old sun-probe
dispatch. No shadow/SSAO listener or `allocateScreenBuffer` is called by native toggles:
that function releases Actor Mask, silCover and area targets (`:1371`, `:1379`).

- Add an AO resource bundle to `LLPipeline::RenderTargetPack` beside existing per-pack
  resources (`pipeline.h:1361`). Main/aux/probe packs must not share AO validity. Prefer
  validity/extent/view-generation in the owning bundle to another unscoped global bool.
- Native master defaults false. Proposed controls: `RenderNativeAOEnabled`, algorithm
  Horizon/Bitmask, radius in metres, thickness in metres, amount, quality, half/full,
  denoise, probe-occlusion enable, feed enable and debug mode. Names are proposals, not
  settings that exist today. Creative and quality ownership is separated in P5.
- Compile stock programs normally; precompile native consumer/pass programs only in
  explicit preparation (startup if Prepare is saved ON), after common permutation
  clearing; link failure disables native only. Runtime
  master/algorithm/amount/debug/probe/feed changes **never call `setShaders`**,
  relink or attach shaders. Do not route through `RenderDeferredSSAO` or rewrite it.
- OFF executes original draw programs, uniforms and dispatch gates. Extra native shader
  objects and admitted resident target reserves are allowed; no native pass or native
  uniform traffic in steady OFF. Do not bind/sample a newly reserved light map in stock
  configurations that originally had none. OFF retains prepared storage so OFF->ON->OFF
  has zero allocations/releases and cannot hitch through screen-pack reconstruction.
- Prepare native-only bundles and required light maps only with Prepare ON, at startup
  or explicit pre-capture extent/format/quality preparation, with both live algorithms using the same storage.
  Pin the admitted plan before capture; include any scalar/bent formats needed by runtime
  probe toggles. Toggle only among prepared resident states. An unprepared request reports
  unavailable/pending preparation and keeps the current valid path; never allocate or
  compile to satisfy a runtime toggle. Startup Prepare OFF reserves nothing native.
  Release on extent/format re-preparation, pool eviction outside a take,
  explicit teardown or GL loss, not on master OFF or temporary unused feed allowance.
- Update screen/Prism allocation, resize, release, eviction, last-close and GL restore
  paths together (`pipeline.cpp:1379`, `:1725`, `:1966`, `:3143`, `:3227`).
  Account all mip bytes, including R32F levels: current format accounting is at
  `indra/llrender/llrendertarget.cpp:89`, allocation at `:378`. An LLRenderTarget extension
  may be needed for explicit mip ownership/accounting; list it in implementation scope.
- Before any view renders, invalidate its AO; validate only after every required pass
  succeeds. An allocation/program/admission failure selects the stock fallback for that
  whole view, never stale main/feed AO. Log requested/effective state once on change.
- Guard framebuffer, viewport/scissor, program, texture bindings/active unit, depth/stencil,
  culling, blending, draw buffers, color masks and relevant sRGB state. Restore on all
  exits. Do not let AO sampling affect sidecar writes, velocity or Actor Mask state.

P1 can retain the original sun producer where it exists (including its now-unused stock green) until
green publication. That pays redundant AO during bring-up but preserves an easy shadow
oracle. A separately precompiled native sun variant with no stock AO evaluation is the
optimization target, gated by identical R/B/A and direct-term results. Do not count the
redundant version as meeting final performance acceptance. P4 removes the remaining
blur-dispatch dependency on the old SSAO flag.

### Startup shader variants and cache gate

`llviewershadermgr.cpp:3638` creates one stock soften program for the current common
configuration. Classic sky is a runtime `classic_mode` uniform and PBR/legacy selection
is a G-buffer flag branch in the same fragment shader (`softenLightF.glsl:214`, `:222`);
HDR is a render/attachment configuration, not three independent soften programs.
Keep those runtime branches. Add **one native soften companion per existing loaded
configuration when Prepare is ON**, supporting scalar/bent, amount/debug and probe enable through uniforms.
Do not create a Horizon × Bitmask × HDR × classic × PBR × preset permutation product.
Across HDR on/off qualification budget at most two native soften cache configurations
with other settings fixed, each covering classic/PBR skies and materials; identical
source/permutation keys may reuse one binary. Do not add an HDR define just to force
two entries. This adds one linked companion per prepared startup/explicit preparation,
rather than eight programs; Prepare OFF adds none.
Existing common keys (shadows/probes/SSR/etc.) still produce their own cache entries.

P1's planned additional linked program count per loaded configuration is **15**:
five depth/gather/denoise/resolve/publish programs, one no-stock-AO sun producer, one
soften companion, and eight OPAQUE/MASK Actor FX beauty companions (2 alpha modes ×
scalar/slot families × unrigged/rigged; loader at `llviewershadermgr.cpp:2691`). Leave
glow and BLEND unchanged in P1. One gather program selects horizon/bitmask by uniform;
denoise count is CPU draw count, not a shader permutation. P3 bent output can use a
runtime output mode with RGBA target binding; budget **zero extra soften companions**
for bent normals. P2's optional BLEND experiment can add four replay beauty companions
plus its explicitly enumerated forward consumer families; record those counts before
enabling the experiment, rather than silently doubling all forward shaders.

These counts imply up to 15 additional cold links and binary-cache records, not an
honest prediction of seconds or bytes without driver measurements. Binary cache loading
and saving already exist (`indra/llrender/llglslshader.cpp:457`, `:1071`). In P1 measure
cold startup and warm-cache startup versus stock, with program/stage counts, cache hits,
disk bytes and resident program memory. Provisional pass/fail ceilings: native additions
≤3 s cold, ≤0.5 s warm, ≤32 MiB extra cache per loaded configuration during preparation;
measure startup delta when saved Prepare is ON. With Prepare OFF require zero additional
native links, cache records and resident program bytes. No compile/link after preparation
on a native-master toggle.
Report failures and optimize/fail preparation; do not move first-use compilation into
a take. Source edits can invalidate existing
shared-helper cache entries as well; measure total startup delta, not just new links.
P4 may add one shadow-only filter companion; include that link/cache record in its
delta. Every later optional forward family must publish its count and repeat the gate.

## 9. Determinism and live-capture performance

### Deterministic inputs and repeatability

`doc/CINE_RELIGHT_QUALITY_BRIEF.md:299` (§6) requires current-frame sampling, fixed pixel
seeds and no temporal accumulation; `:318` requires OFF image diffs and inert resources.
For AO there is no light ID: seed only integer output-pixel coordinates and an explicit
fixed seed. No frame number, clock, channel number, camera UUID, `noiseSine`, `ll_frand`
texture regeneration or velocity. Freeze XeGTAO's noise index to its non-temporal value
or use its spatial low-discrepancy sequence with fixed index. An arbitrary pixel hash
is a diagnostic alternative, not a qualified noise choice. Filtering is spatial only.

For half resolution define exact source texel centers; for tiles use global final-output
coordinates. Same current inputs/settings/extent/seed on the **same GPU/driver** must
produce bit-identical AO readbacks, including after camera cuts, feed scheduling changes
and process restart. No AO warmup history is required. This does not imply bit-identical
whole-viewer beauty across GPU vendors or identical camera alone: texture streaming,
animated surfaces, native probe fades, sky changes, AA jitter and governor state can alter
the G-buffer. Record/freeze those inputs and disable temporal AA/exposure/noisy post
effects for the test oracle. Native AO must not depend on or enable TAA to reach its
quality claim. Compare raw AO as well as final images.

**CPU variance sizing first (P1-C):** before selecting production shader presets, run a
CPU current-frame slice/sector sampler and the actual proposed spatial kernels against
numerically integrated plane/corner/contact references. Sweep deterministic seeds,
256 sub-pixel offsets, slopes, distances, full/half/odd extents and both algorithms.
Measure raw error covariance C and propagated resolved error variance `w^T C w`;
report bias separately. Compose all denoise/upsample weights, accounting for overlap
and bilateral rejection: neither `9 * passes` nor depth taps equals independent samples.
Fit correlation/variance bounds from the worst fixture, then choose slices, steps and
passes to meet the preset's declared gate **before GPU implementation/timing tuning**.
Record model inputs and reproducible CPU output; a failed model blocks that preset.

For a concrete planning calculation, use raw sigma `0.09 * sqrt(8 / T)`, T depth
gathers/pixel, covariance inflation 1.25, and a residual correlated sigma floor 0.002.
For a uniform plane interior, convolve normalized `[1,1,1]/3` p times along each axis;
`E = 1 / (sum(k_i^2))^2` is the 2D effective spatial sample count. Predict
`sigma_resolved = sqrt(0.002^2 + 1.25 * 0.09^2 * (8/T) / E)`.
This analytical CPU calculation was evaluated for the table below; it is a sizing
assumption, **not a fitted SL covariance model or a GPU pass claim**. Equal 3×3 weights
are the plane-interior approximation to the proposed bilateral filter; edge rejection,
half-res interpolation, shared depth samples and angular correlation can reduce E.
The full CPU reference sweep must replace these assumptions with conservative measured
bounds before freezing presets. Do not credit upsampling as extra independent samples.

The original Low (T=8, p=3) has E=26.73 and predicted sigma 0.0196, so a universal 0.005
gate is withdrawn. At unchanged Low cost it cannot plausibly claim that gate. Balanced
also has an explicitly relaxed gate; users needing ≤0.005 select qualified High or
Cinematic and pay their cost. If the fitted model misses even a relaxed gate, increase
work and re-admit/relabel, or withhold the preset; never silently relax its published gate.

**No-TAA spatial floor:** static screen seeds prove repeatability, not spatial stability
under camera motion. Intel's [denoising discussion](https://github.com/GameTechDev/XeGTAO#denoising)
describes a spatial filter supplemented by TAA when present; its
[header version notes](https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.h)
call out the one-pass case with TAA. This plan therefore requires **at least three
complete edge-aware spatial denoise passes** without TAA, before depth/normal resolve;
the model-sized candidates use Low 3, Balanced 4, High 7 and Cinematic 8. One pass
means one full gather-extent ping-pong draw, not an axis
or the resolve. This is a conservative SL design floor, not a claim that Intel proves
three passes sufficient. Every preset must pass its declared gates below; raise spatial samples
or passes, redesign noise/resolve or withhold that preset if it fails. Never use temporal
history, TAA or capture postprocessing to hide swimming. Blur cannot erase P2's thin-gap gate.

- **Plane-variance gate:** at each preset, full/half, 1080p/4K and odd extent, use a
  256×256 interior ROI on an unoccluded plane at front-facing/30°/60° slopes and 1/3/10 m.
  Across the resolved visibility pixels require mean ≥0.99, standard deviation ≤0.002
  and peak-to-peak ≤0.01, over 256 sub-pixel camera positions. Also use a uniformly
  partially occluded plane ROI to detect noise hidden by saturation: standard deviation
  of error from its numerical reference ≤0.025 Low, ≤0.010 Balanced, and ≤0.005
  High/Cinematic. These are explicit quality differences, not universal 0.005 compliance.
  Report mean error/bias too; require absolute mean error ≤0.005 for every preset.
  Store raw and resolved distributions, modeled covariance and measured effective E.
- **Camera-pan gate:** at fixed exposure, no AA jitter/TAA and static geometry, pan in
  horizontal/vertical/diagonal 0.125/0.25/0.5-pixel increments for 256 frames, then reverse,
  at 1080p and 4K. For the same screen pixel on valid overlapping receiver interiors,
  measure `abs(AO[t](p)-AO[t-1](p))`: maximum ≤0.02, p95 ≤0.005, and maximum no larger
  than stock SSAO's maximum +1/1024 on the identical sweep. Apply these absolute limits
  only to reference-stable interiors. Compare visibility before
  stock look shaping, after each algorithm's spatial filtering; freeze stock noise as
  below and record its measured maxima. For **all contact-crease/rail/finger/corner pan
  ROIs**, including continuously visible receivers, use excess delta versus a converged
  current-frame reference: `abs((AO[t]-AO[t-1]) - (Ref[t]-Ref[t-1]))`, maximum ≤0.02
  and p95 ≤0.005. This signed-delta residual permits genuine reference contact changes
  above 0.02; an absolute AO-delta 0.02 cutoff does not apply to these ROIs. Label
  newly revealed pixels/discontinuities separately and apply the same reference-residual
  gate wherever both frames have valid reference correspondence.
  A world-aligned diagnostic difference may be computed offline, never used by runtime AO.
  Hardware-captured 120-second pans must also show no screen-locked pattern or swimming;
  perceptible artifacts fail even if aggregate statistics pass. Repeat the gates after
  bitmask and bent changes and for every degraded preset eligible during a take.

### Requested presets and budgets

These are **CPU-model-sized candidates and proposed SL validation targets, not measured
GPU results**. Steps below mean
samples **per side per slice**; sector count is 32 for every P2 preset. All use fixed
spatial noise and depth-aware resolve. High is a starting default for fine avatar detail;
half-res modes are explicitly a speed/detail/noise tradeoff. The CPU sizing gate precedes
GPU cost qualification; a timing budget never overrides the declared quality gates.

| Preset | Gather | Slices × steps/side; T | 3×3 denoise passes; E | Modeled sigma / declared partial-plane gate | Main AO p95 target at 1080p / 4K |
|---|---|---|---|---|---|
| Live Low | ceil-half | 2 × 2; 8 | 3; 26.73 | 0.0196 / ≤0.025 | ≤0.9 / ≤1.9 ms |
| Live Balanced | ceil-half | 4 × 4; 32 | 4; 35.13 | 0.0087 / ≤0.010 | ≤1.8 / ≤4.0 ms |
| Detail High | full | 6 × 6; 72 | 7; 60.24 | 0.0048 / ≤0.005 | ≤3.0 / ≤9.0 ms; 4K needs raised gather budget |
| Still / Cinematic | full | 8 × 8; 128 | 8; 68.62 | 0.0036 / ≤0.005 | ≤5.0 / ≤16.0 ms; explicit still budget |

The Low/Balanced targets cannot claim the 0.005 partially occluded-plane gate within
these candidate work budgets. All presets still share the stricter open-plane, bias,
thin-gap, reference-residual pan and visual swimming gates. More passes widen filter
support (High 7, Cinematic 8 gather pixels per axis before resolve); the CPU contact
fixtures must reject erased creases or thin gaps. Additional passes reuse ping-pong
targets, so bundle bytes below do not increase merely with pass count.

Targets include depth prepare, gather, spatial filters, resolve and publish, on a declared
RTX-3070-class test GPU; log actual GPU/driver, clocks, scene, AA, divisor, feeds and
bent on/off. Bent mode gets a provisional +35% budget; reject a larger increase until
profiling explains it. Intel's published example timings are only a plausibility check:
full-resolution high was approximately 0.56 ms at 1080p on RTX 2060 and 1.4 ms at 4K on
RTX 3070, with directional output roughly +25%; those are different hardware/API tests,
not an SL guarantee. [Intel XeGTAO measurements](https://github.com/GameTechDev/XeGTAO)

Use a deterministic AO work ceiling based on `pixels * slices * 2 * steps`, plus a
separate fixed bound for depth/filter/resolve work and bytes. Example starting gather
ceiling: **160 million gather depth samples only** for main + feeds. It excludes
denoise, depth preparation and resolve/publish. Low's 27 denoise neighbors/pixel are
3.375× its 8 gather taps; they are charged only under the combined **2,000 M
tap-equivalent** ceiling, never hidden in or claimed bounded by the 160 M subceiling.

| Candidate | Gather reads, M at 1080p / 4K | Denoise neighbor reads alone, M at 1080p / 4K | Gather-only 160 M admission, main alone |
|---|---|---|---|
| Low | 4.15 / 16.59 | 14.00 / 55.99 | Both fit |
| Balanced | 16.59 / 66.36 | 18.66 / 74.65 | Both fit |
| High | 149.30 / 597.20 | 130.64 / 522.55 | 1080p fits with little feed headroom; 4K fails |
| Cinematic | 265.42 / 1,061.68 | 149.30 / 597.20 | Both fail; explicit still plan only |

Denoise entries count 9 AO neighbor fetches per pass only, a **lower bound**, not total
tap-equivalents: add edge/depth/normal guidance fetches, each target component write,
mip preparation, gather metadata writes, full-res resolve and publication. Bent output
writes four components instead of one; count any additional guidance fetches separately
and publish exact coefficients from final loops.
The combined ceiling can reject a candidate even when its gather fits. An explicitly
qualified still plan may raise the gather subceiling to **1,200 M**; combined 2,000 M
work, resident bytes and timing still apply unless separately declared and qualified.
Full 4K High is therefore unavailable under the default live gather cap, and Cinematic
is never an automatic live fallback. A modeled 0.005 preset is not promised within
the Low/Balanced live budget at 4K.
Use 64-bit arithmetic and ceil-half extents. Only prequalified presets meeting the
spatial gates may be admitted; do not cut denoise below its floor or improvise a sparse
sample count to fit. Apply the combined degradation order below before capture, never
feed GPU timing into pixel quality during a take. Preserve requested/effective values.

Main reservation comes first. Feeds receive fixed allowances from the remainder, bounded
by eight **actual AO view passes**, including repeated Gate renders, as Step 6 does
(`alareashadowmath.h:118`, `pipeline.cpp:26956`). Frame identity may reset accounting only.
No redistribution based on scheduling/visible content. Keep the AO and area-shadow
subceilings and enforce the combined budget below as an additional hard gate.

Approximate additional target storage, excluding existing G-buffer/light buffers,
allocator overhead and old/new overlap during transitions (N = full pixel count):

| Variant | Approximate bytes | 1080p / 4K |
|---|---|---|
| Scalar full | R32F depth chain ≈5.33N + R8 edges N + raw/scratch/resolved 6N | 24.4 / 97.6 MiB |
| Scalar half | depth ≈5.33N + edges 0.25N + raw/scratch N + resolved 2N | 17.0 / 67.9 MiB |
| Bent full | depth ≈5.33N + edges N + three RGBA16F targets 24N | 60.0 / 239.9 MiB |
| Bent half | depth ≈5.33N + edges 0.25N + two half RGBA16F 4N + resolved 8N | 34.8 / 139.1 MiB |

Five levels are slightly below the infinite 4/3 mip-chain estimate above. Odd extents
must use the actual level sums. Eight resident 2048² scalar-full bundles would cost
approximately **394.7 MiB**, already failing the **256 MiB feed-AO subceiling**, before
reserved light maps, Relight or feed beauty targets. Eight identical-size cameras actually
reuse one exact-size scratch bucket in this checkout; this example is a conservative
resident-bundle stress case, not a claim that camera IDs each own a pack. Charge each
real resident target once for memory, but every actual rendered view pass for work.
Include pinned preview/capture targets that coexist, not just the reusable scratch pool.

### Hard combined admission: AO + Relight soft shadows + VCam

P1 establishes one pure, allocation-free preflight ledger; P3 extends it to native feeds.
Use `alareashadowmath.h:78`, `:118`, `:134` as the pattern: 64-bit integer bounds, reserve
main requested-cap work first, fixed remaining feed allowances, count repeated Gate
renders, and reject a view's admission before the take when its minimum cannot fit.
Do not borrow an idle
main reservation or another feed's allowance based on timing, cadence or visible lights.
Use requested Relight light cap for reservations, not the momentarily selected set.
Divide remaining work into eight fixed pass-slot allowances (integer remainder unused),
with AO + Relight + feed rendering charged together to each allowance. Map prepared
passes to slots by the stable capture-slot ordering below; each repeat render consumes
its own pass slot. Memory is admitted by actual resident size buckets, not divided by
camera count; reserve its main share before admitting feed buckets.

The following are proposed release ceilings, not current enforcement or measurements;
machine settings may lower them. Raising them is a separately qualified capture plan.

| Gate or diagnostic | Accounting / consequence |
|---|---|
| Combined resident and transition peak ≤768 MiB; feed AO subset ≤256 MiB | Sum **main + all resident feeds** AO depth/mips/edges/ping-pong/resolve, Relight gather/scratch/visibility for every channel, native-required `deferredLight` reserves, and all VCam G-buffer/depth/beauty/light/water/capture/preview targets. Include post attachments, retained OFF/prepared bundles, alignment/driver allocation estimates and simultaneous old/new resources. Count shared depth/targets once. Existing main beauty/G-buffer is a baseline outside this incremental ledger; it still counts in total VRAM/headroom. Target success alone is not admission. |
| Combined scheduled texture work ≤2,000 million tap-equivalents/frame | For AO **and** Relight main/feeds, sum worst-case depth gathers (including mip prep and hit refinement), all filter/resolve/publish reads/writes; add every scheduled VCam fullscreen/post/copy pass at its reserved dimensions. One texture read or render-target component write is one equivalent; dynamic-loop passes use their maximum bound. Publish the per-pass coefficient table from production shader loops; an unknown coefficient is a failed admission. Retain live AO gather ≤160 M (explicit qualified still plan ≤1,200 M) and Relight's own ≤1,500 M iteration subceilings as additional limits, not substitutes for this sum. Denoise is excluded from the gather subceiling and included here in full. |
| Feed raster work: **report-only** thresholds 4,096 draw calls / 8 M submitted triangles per frame; hard ≤8 actual AO/Relight feed passes | Geometry cost depends on content and is not bounded by texture taps. Record counts, threshold overruns and frame-time impact for qualification, including repeated Gate renders. They may fail a pre-take workload qualification but never reject, freeze, resize or reschedule a feed mid-take. Do not make per-frame admission depend on draw-list contents. The eight-pass cap is a static schedule bound; reject a ninth scheduled pass before the take, with a runtime guard against schedule corruption before drawing/allocating. |
| Measured combined GPU delta p95 ≤4 ms at 1080p60 / ≤6 ms at 4K30 | Compare AO + Relight soft shadows + all admitted VCam work enabled versus the same main scene with those costs disabled. Also pass the full-frame 2 ms headroom and present-interval gates below. The structural limits do not predict milliseconds; a plan that fails either timing or structural limits fails qualification. 4K60 needs its own measured qualification. |

**Deterministic degradation, before preparation/take:** reserve the requested main AO
and main Relight plans first. **Use capture slot order only**: lower numbered occupied
slots have precedence, higher numbered slots degrade first. There is no explicit capture
priority field in `llprismlens.h`, and this plan adds none. Pin the slot map; repeated
Gate-source renders use their prepared source-slot order and fixed occurrence order,
not runtime arrival order or object IDs. For each highest numbered admitted feed:
(1) bent -> scalar AO, (2) step down through qualified
AO presets to Low, (3) reduce feed Relight samples, then steps, then half resolution
using its existing pure workload planner, (4) disable native feed AO (charge any stock
fallback work too), (5) disable feed Relight, (6) if capture cost still prevents admission,
return an explicit **Needs capture-governor reprepare** result, (7) reject that feed's
admission before the take if no governor plan fits. AO does not change feed resolution
or cadence. Recompute memory and worst scheduled-frame work after each step; cadence
never reduces resident bytes or the peak where all feeds coincide. Skip ineligible
sizes/presets; keep requested values in status.

**Capture-governor precedence:** the existing capture governor is the sole owner of
resolution scale and cadence (`llprismlens.h:353–385`, `mEffectiveResolutionScale`,
`mCadenceEntitlementHz`, `mUserResolutionScaleCeiling`, `mAdaptiveEnabled`). Outside a
take it may choose a new plan under the user's capture ceilings; AO/Relight preflight
then admits resources/work for that snapshot. Step 6 is a handoff, not a second governor
or an AO override of capture settings. Iterate reprepare outside capture until both
plans fit, or reject capture admission with a reason. Freeze the final resolution,
cadence entitlements, slot map and worst-frame schedule at take start; suspend adaptive
governor changes until the take ends. Status may continue reporting timing/pressure.
This requires explicit governor pin/unpin integration in P3, not an assumption that
today's runtime fields already freeze it. Neither planner may overwrite the other's
plan mid-take; catastrophic GL/resource loss ends the qualified take with a logged
fallback, rather than silently changing quality or freezing selected feeds.

Only after all feed reductions/rejections may main lose bent output, step down qualified
AO presets, then reduce main Relight samples/steps/half using its existing pattern; if
still over, reject the native/Relight capture plan and report baseline fallback. Memory
pressure also releases excluded bundles **before** capture; scalar/half transitions that
would exceed the peak limit must free old storage first. No quality or allocation churn
during a take. All retained residents, including an AO-disabled but prepared feed, still
count: disabling execution alone cannot solve a byte overrun.

Acceptance must cover 1/2/4/8 feeds with AO + Relight together, mixed real pool sizes,
2048² stress accounting, HDR/non-HDR, main 4K bent, odd extents, pinned preview/outputs,
repeat Gate renders and a deliberately over-cap plan. Log requested/effective plans,
per-owner bytes/peak bytes and scheduled work; every ledger entry and every ceiling
must PASS, or the deterministic reduced plan must fit and be visibly reported. Reorder
the view schedule: effective quality and main reservation remain identical. Mutations
that omit Relight/feed beauty bytes, count IDs rather than passes, hide transition
overlap or redistribute unused reservations must fail. Hardware recording is qualified
against this combined plan, not against an AO-only wall benchmark.

### Frame limiter and VRAM governor

The limiter waits at frame end and counts VSync/background time already elapsed;
slow frames skip waiting (`llappviewer.cpp:1811`). It cannot recover GPU time spent
on AO. Its sleep is tagged `RENDER_SLEEP` so AutoTune does not treat intentional
waiting as slow rendering (`:1816`). Never use that sleep or a displayed capped FPS
as AO cost. Measure GPU timestamps and CPU/frame intervals independently.

For hardware recording, qualify 60 fps (16.67 ms) and 30 fps (33.33 ms) explicitly.
On settled representative scenes, require at least 2 ms measured frame headroom at
the selected rate, p99 inter-present interval ≤1.1× period and fewer than 0.1% intervals
>1.5× period over a 120-second run. If the AO-off scene already fails, report baseline
failure and AO delta rather than labeling AO a stable-frame-rate solution. Run an
uncapped performance sample as well as capped recording; include populated event/crowd
scenes and multiple feeds, not just a wall fixture.

The VRAM governor treats render targets as fixed allocations; render-target bytes are
already inside texture accounting (`llviewertexture.cpp:654`). It can raise the effective
cap for the fixed-allocation floor (`:673`), and capture texture protection yields under
pressure (`:501`, `:524`). New AO buffers can therefore lower streamed-texture quality
or cause pressure even if their own allocations succeed. Use existing accounting exactly
once, add pre-capture memory admission/status, and qualify pressure/recovery. Do not
silently change thickness/radius or oscillate quality based on governor timing. Pin the
effective plan during a take; catastrophic allocation loss ends the qualified take
with a logged whole-view fallback and capture-quality warning, never stale data.

## 10. Phased implementation with measurable gates

Effort is experienced graphics-engineer days, including automated fixtures and review
but excluding user screenshot turnaround/hardware availability. Phases are independently
reviewable; the order is deliberate. Gates below are future work, **not tests run by this
planning task**. No check may silently pass when a GL capability or image oracle is absent:
report INCONCLUSIVE and do not ship the corresponding claim.

### Common harness contract (all phases)

Extend the style of `scripts/code_tools/relight_area_check.py:1`: a real GL context linking
production shaders (`:9`), controlled depth/normal/material inputs, framebuffer readbacks,
and current-camera negative controls (`:510`). Add proposed `ao_variance_check.py`
(CPU reference/covariance sizing before preset freeze), `ao_bitmask_check.py`,
`ao_dispatch_check.py` and pure math TUT cases. Dispatch/ownership tests must exercise
production routing as `scripts/code_tools/relight_vcam_dispatch_check.py:14` does, not a copied mock that
publishes validity manually. Its executable mutation pattern (`:581`) is the model.

Every phase inherits:

- Native OFF versus clean baseline at this HEAD: byte-equal raw light-map and stable
  pre-post beauty readbacks, SSAO true/false × shadows 0/sun/local × HDR/non-HDR ×
  classic/PBR skies. Also ON->OFF restoration, water/alpha/replay, and resize/camera cuts.
  For configurations with no baseline light map, compare the original neutral inputs
  and beauty; the new reserved texture must not introduce a stock read/dispatch.
  Freeze baseline noise for equal-input comparisons with the production-inert hook below;
  native reproducibility also tests across restarts. Compare pre-tonemap values.
- Master/algorithm/amount/debug/probe/feed runtime toggles: **zero shader reloads,
  compiles, links, allocations/releases, screen-pack rebuilds and legacy-setting writes**
  for a prepared resident plan. Count calls, do not infer from UI smoothness. Resource
  preparation happens only with Prepare ON, at startup or an explicit extent/format/plan
  transition before capture; Prepare OFF stock startup allocates/links nothing native.
- Settled identical extent/plan for 300 frames and a 120-second take: **0 target/texture
  allocations and releases per frame**. Master OFF: zero native passes and native uniform
  uploads; reserved resident bytes stay unchanged, budgeted and disclosed. Explicit native
  teardown releases native-only bundles; shared light-map lifetime follows explicit reservation
  and its stock/DOF users. Check shader objects separately from targets.
- Repeat same GPU inputs 100 times and after view/cut/schedule permutations: byte-equal
  AO output, finite/bounded values, no history/velocity reads. No CPU/GPU synchronous
  query waits in the pass. Full GL state and main-view sentinel restored after all exits.
- Native ON direct sun and local-light inputs **before** classic compatibility recombination
  are unchanged; emissive-only pixels and graphic overlay inputs are unchanged. For legacy
  classic skies capture `linear_to_srgb(sun_contrib) * sunlit_linear * 0.7` immediately
  before `softenLightF.glsl:299`; for PBR capture `finalSun` before the nonlinear sum in
  `deferredUtil.glsl:1763`. Verify sun visibility, shading N, BRDF and emitter parameters
  independently. Neither final-beauty subtraction nor legacy `debug_direct` is a direct
  equality oracle; stock SSAO has the same nonlinear recombination dependency. GPU term
  readbacks use exact equality where programs are identical; native variant arithmetic
  differences must remain within 1e-6 relative/absolute FP32 tolerance and be explained.
  No tolerance permits an AO-dependent direct-light reduction. Test sun-only, point-only,
  projector-only, rig-only, emission-only and indirect-only scenes independently.

**Stock noise freeze isolation:** `pipeline.cpp:3324` uses `ll_frand()` to build the stock
noise texture. Never seed/replace global `ll_frand`, change call count/order or install a
shipping debug setting for this test. Prefer same-process baseline/OFF draws sharing the
actual captured noise bytes. If cross-process fixtures need capture/replay, guard the
texture-upload interception with a dedicated test-target-only compile definition, absent
from every shipping configuration; it defaults to pass-through and is activated only by
the harness. Even in that target execute the original RNG draws before test upload so
downstream RNG state is unaffected. Use the identical captured bytes in clean-baseline
and candidate harness runs. Prove the inactive hook uploads the original bytes unchanged,
and inspect release compile definitions/preprocessed path to prove it is compiled out.
The primary OFF byte-identity gate also runs without any hook active. Fixing the native
seed never changes stock noise generation or invalidates the OFF oracle.

### P1 — Horizon baseline and native runtime framework

**Effect:** cleaner spatial AO in opaque/MASK world and avatars, using XeGTAO-style
slices, fixed noise and edge-aware denoise. Master OFF restores stock SSAO exactly.

**Files:** `pipeline.cpp/.h` including **main allocation `:1448`, Prism completeness
`:1770` / allocation `:1830`, producer dispatch `:27310`, preview `:27295` and cube/view
gates**, `bindDeferredShader :25082` and explicit light/DOF binding validity, targeted
preparation/lifecycle and combined admission ledger;
`llprismpreview.cpp` for preview-policy verification at `:1453` (preserve its override);
`llviewershadermgr.cpp/.h`; proposed
`alnativeaomath.h`, `class1/deferred/nativeAOUtil.glsl`, `nativeAODepthF.glsl`,
`nativeAOGatherF.glsl`, `nativeAODenoiseF.glsl`, `nativeAOResolveF.glsl`,
`nativeAOPublishF.glsl`; native permutations of `class3/deferred/softenLightF.glsl`
and `class2/deferred/sharedActorFxPbrF.glsl`; `app_settings/settings.xml`;
`indra/llrender/llshadermgr.cpp/.h` if reserved uniforms are needed;
`indra/llrender/llrendertarget.cpp/.h` if mip ownership requires it;
`indra/newview/licenses-win32.txt`, `indra/newview/licenses-linux.txt`,
`indra/newview/licenses-mac.txt`, with existing `viewer_manifest.py` package mappings
verified; the proposed harness files and test-target compile-definition plumbing for
noise capture/replay if needed. Prefer a shared native-only
utility over changing stock helper expressions. No full Director UI yet; debug settings
and an effective-status diagnostic suffice for the first A/B.

**Risk / effort:** high (GL conventions, light-map allocation/dispatch, OFF isolation,
spatial stability, combined ledger and target preparation), **12–17 days**. Light-map
target work and its preview/cube regressions belong here, not deferred to P4.

**Acceptance:**

- Plane AO ≥0.99, sky visibility exactly 1, no NaNs; interior corner is measurably darker
  than the plane (visibility difference ≥0.10 in a specified radius/geometry fixture).
  A CPU hemisphere reference gives mean absolute visibility error ≤0.05 on known opaque
  plane/corner/sphere fixtures at High; save the fixture dimensions and radius with results.
- Every advertised preset first passes the CPU covariance/reference sizing sweep, then
  §9's **per-preset** partially occluded-plane sigma gate, common open-plane/bias gates,
  stable-interior pan gate against stock and contact-crease **reference excess-delta**
  gate, with TAA disabled. Candidate denoise draws: Low 3, Balanced 4, High 7, Cinematic 8.
  A deterministic but visibly screen-locked/swimming image fails. Cost tables and
  admission coefficients must match the fitted, qualified preset, not these assumptions.
- **Preparation gates:** fresh stock startup with Prepare OFF has zero additional native
  targets, native links/cache records and reserve-map bytes. Unsupported GL/extent rejects
  preparation without partial resources. Prepare ON allocates each missing target once,
  clears new maps white and publishes Prepared only on complete success; repeat the same
  request without allocations. Verify explicit failed-admission/failed-link rollback.
  Sweep live VRAM/governor pressure after preparation: static support, reservation and
  pack IDs never flip; no `allocateScreenBuffer` call, automatic retry or mid-take teardown.
  Prepare OFF tears down outside capture only; scenes/presets cannot enable Prepare.
- **Reserved OFF content:** with no stock producer, first prepared OFF must bind white
  through soften beauty/debug and explicit lighting bindings. Repeat ON->OFF with dirty
  RGBA sentinels, previous DOF scratch, reuse/resize/GL restore and HDR/non-HDR. Beauty and
  debug equal the original white-fallback baseline; no native pass/clear each OFF frame.
  DOF combine reads only completed current CoF/beauty scratch and matches stock output.
  Mutations removing allocation white clear, bypassing OFF binding validity, or accepting
  Lighting/Neutral as valid DOF scratch must fail their respective ownership gates.
- **Allocation/dispatch hole:** stock SSAO=false × shadows=0 × DOF=false × debug=0,
  native ON produces a complete, correctly sized light map and non-neutral G on a corner;
  R/B/A stay exactly neutral. Repeat HDR/non-HDR, odd/divisor sizes and 100 prepared
  OFF/ON toggles: zero allocation/release, `allocateScreenBuffer`, shader-reload/link or
  legacy-setting calls; Actor Mask, silCover and area target IDs/depth sharing unchanged.
  Assert both allocation gates and the dispatch gate independently; disabling any one
  must fail. Verify the future feed preparation predicate with scratch-pack fixtures in P1;
  actual native feed exposure waits for P3 isolation.
- With stale nonwhite sentinels preloaded, preview remains neutral under
  `sPrismPreviewRender` even when master ON and shaders capable; `gCubeSnapshot`, hero
  mirror, HUD, impostor and reflection views execute no native passes/lookups. Exercise
  flags independently and nested early exits; restore main validity and view state.
- DOF ON/OFF: CoF scratch can overwrite `deferredLight` only after lighting consumers;
  next native view reinitializes it, retains correct corner AO and shows no previous-frame
  CoF sentinel. No `aoResolved` alias with DOF or area targets.
- Combined-budget preflight rejects over-cap storage/work before allocation/draw,
  including reserved OFF targets. Shipped platform `licenses.txt` contains the complete
  pinned XeGTAO MIT notice. Startup counts/time/cache pass §8's shader gate.
- Full/half and odd 63×47, 1920×1080, 3840×2160 reconstructions have ≤1 full-res pixel edge
  alignment error. Near/far/FOV/off-axis tests reconstruct a known plane within depth-format
  precision (explicitly derived tolerance), without a screen-space radius sign flip.
- Deferred versus shared OPAQUE/MASK replay indirect result agrees within one RGBA8
  quantization step or the corresponding FP tolerance. Blend remains the documented baseline.
- Actual stock R/B/A light-map channels remain byte-identical after green publication.
  Meet timing budgets after replacing redundant stock AO computation, not before.
- **Mutations that must fail:** reverse GL z convention; use main projection in another
  view; remove gather dispatch; publish into R as well as G; force validity before a
  failed allocation; multiply beauty instead of irradiance; call `setShaders` on toggle;
  allocate the scratch target each frame. Also require OFF byte gates above.

### P2 — Visibility bitmask and finite thickness

**Effect:** reduced dark halos behind narrow geometry, lit gaps between railing bars
and fingers, with a controllable thickness compromise. Horizon remains a diagnostic
comparison option. AO is still indirect-only.

**Files:** native gather/util/math, settings, harness/TUT fixtures; native alpha consumer
variants in `class2/deferred/alphaF.glsl`, `pbralphaF.glsl`, `sharedActorFxPbrF.glsl` and
`class3/deferred/materialF.glsl` only
for the optional receiver experiment. No transparent caster or new G-buffer layout.

**Risk / effort:** high (sector math, finite-thickness leaks, sparse data and motion), **7–10 days**.

**Acceptance:**

- Pure interval oracle: empty/full masks, endpoint rounding, zero/32-bit spans, overlap
  union, both slice directions and grazing normals. Union is order independent; duplicating
  a sample cannot add occlusion twice. Angular/cosine-weight integral agrees with dense
  numerical integration with mean error ≤0.02 and maximum error ≤0.05 for a fixed sweep
  of analytic angular intervals at 32 sectors. Publish the sector-boundary quantization
  error separately; revise the approximation/sector resolution if these gates fail.
- In synthetic railing and finger-gap fixtures, 1–3 cm foreground blockers sit at a
  specified separation from a background receiver; reference-open gaps retain ≥0.90
  visibility, with no halo extending >1 full-res pixel beyond the reference boundary
  at High. A contact junction still loses ≥0.10 visibility. Record camera/radius/thickness,
  compare both raw and filtered results, and repeat at 1/3/10 m and both resolutions.
- Fence/lace alpha-test textures retain all reference-open holes wider than 2 pixels
  at full resolution. Half-res subpixel failures are reported and cannot qualify as
  equivalent to High. Solid-wall/corner fixtures remain within 0.05 mean absolute AO
  error; a smaller thickness cannot be accepted solely because it brightens every scene.
- Thickness sweep 0.01/0.03/0.10 m is finite and stable, with explicit thick-wall leakage
  examples. Repeat thin cases with LOD0 then mip sampling; coarse LOD cannot erase the
  lit-gap gate. A fixed-pixel denoiser must not reintroduce a horizon-shaped halo.
- Optional blended receiver gate: foreground hair over an occluded wall uses neutral AO
  when depth mismatches; matching replay surfaces agree. Test legacy/PBR hair, lashes,
  lace and water. If this gate fails, ship the opt-in disabled and label the limitation.
- **Mutations:** infinite-thickness/back angle = horizon; reversed front/back angles;
  AND instead of OR; shift by 32; cosine treated as angle; mask reinitialized per sample;
  duplicate samples counted twice; bilateral rejection disabled; alpha blindly samples
  wall AO. Each must trip a named fixture. Determinism/OFF/reload/allocation gates remain.

### P3 — Bent normals, probe specular occlusion and view isolation

**Effect:** indirect light follows open directions; probe reflections lose implausible
brightness inside close creases while direct highlights remain. Optional native AO in
real VCam camera feeds uses each camera's own depth and projection.

**Files:** native gather/filter/resolve formats and math; native permutations/entry points
in `class3/deferred/reflectionProbeF.glsl`, `class2/deferred/reflectionProbeF.glsl`,
`class1/deferred/deferredUtil.glsl`, `class3/deferred/softenLightF.glsl`, forward/replay
consumers; pipeline target-pack lifecycle and `begin/endPrismAuxiliaryState`; viewer shader
manager, combined AO/Relight/VCam ledger and admission wiring, `llprismlens.h` and the
capture-governor implementation for snapshot/pin/unpin integration, settings and AO
dispatch/VCam harnesses. Include the separate legacy gloss/environment overloads and class-2 conversion
policy in §5. If texture-unit/link limits are exceeded,
reduce native bindings or fail native only, not stock shaders.

**Risk / effort:** high (probe/SSR separation, artistic compatibility, auxiliary state),
**10–15 days**, including a separately gated **3–4 day feed/budget substep** within this estimate.

**Acceptance:**

- Open plane bent N dot shading N ≥0.999; asymmetric blocker bends toward the known
  opening; unit length error ≤1e-3 after resolve; zero moment never yields NaN. Symmetric
  blockers do not introduce a preferred screen direction. Repeated output is byte-identical.
- Roughness sweep 0.05/0.25/0.5/1.0 × view-angle sweep: probe specular factor [0,1], open
  case =1; a blocked-cone fixture reduces only its expected indirect reflection. Compare
  materialAO 1/0.5 and native visibility 1/0.5 against a composition oracle to catch squaring.
- Direct sun glint and local-light glint remain unchanged. SSR-only and hero-only fixtures
  hold ambient inputs equal and are unchanged by probe-only occlusion; compare direct
  inputs before legacy energy/recombination heuristics. Test `glossenv` and `legacyenv`
  independently across spec.a=0/>0, envIntensity=0/0.5/1, SSR on/off and hero replacement.
  A post-mix AO multiply must fail; class-2 sRGB round-trip at factor=1 must be bypassed.
  Probe capture buffers are unchanged, and classic
  sky/probes-disabled fallback is finite and preserves the intended ambient-only scope.
- Actor FX shared replay parity; Tron/rim/emissive and silhouette invariants; every
  receiver policy tested under HDR and non-HDR without misreading flags/carriers.
- Feeds: 1/2/4/8 views, odd/divisor sizes, same-size Gate source cut, repeated source renders,
  allocation failure at every target, resize, eviction and last close. Main AO sentinel
  unchanged, no stale validity, correct incoming camera, ≤1 pixel reconstruction alignment,
  zero steady allocations/releases, exactly one allocation per changed target. Ninth actual
  pass is rejected before allocation. Main+feed work and resident bytes stay within ceilings.
  Pass the combined AO + Relight + VCam ledger/timing gates and every excluded-view/DOF
  gate from P1 with nested feed/preview/hero passes. All qualified feed presets pass their
  declared no-TAA variance gates and the common pan/visual gates. Change draw/triangle
  counts mid-take past report thresholds: feed cadence/quality stays pinned and updates
  continue. Verify slot-order degradation, capture-governor handoff and pin/unpin, plus
  timing/VRAM pressure changes that cannot mutate an admitted take. Degraded feeds cannot
  ship visible swimming.
- **Mutations:** bent N used as direct BRDF normal; AO applied twice through ORM; specular
  occlusion after SSR mix; shared replay skipped; feed aliases main target; main validity
  restored too early/not restored; source switch reuses old result; budget counts feed IDs
  instead of actual renders; partial allocation publishes validity. All must fail.

### P4 — Independent AO and sun-shadow filtering

**Effect:** changing AO no longer changes sun shadow softness. Sun filtering remains
usable with native AO off/zero, with a clearly defined compatibility mode.

**Files:** `pipeline.cpp/.h`, native sun producer variant, `class1/deferred/blurLightF.glsl`
or new native shadow-only filter, shader manager, settings, shadow/AO fixtures. Legacy
SSAO path and its old semantics remain untouched while compatibility is offered.

**Risk / effort:** medium-high (existing looks depend on blur coupling), **4–6 days**.

**Acceptance:**

- Native mode AO algorithm/amount/denoise changes do not change the sun channel; shadow
  soften changes do not change native AO. Verify exact channel readbacks, not visual judgment.
- Modes 0/1/2/3 have documented migration: stock-equivalent shadow kernel, pixel sigma,
  world-mm sigma and none. Preserve meaningful pixel/mm controls as sun settings. Retire
  the label/workaround only after parity; do not delete users' saved values prematurely.
- Native mode works with legacy `RenderDeferredSSAO=false`, native shadow quality 0 or
  enabled, with no shader reload. Native master OFF still restores old blur gating and
  byte-identical stock output. An explicit future migration to independent shadows is
  versioned/opt-in so old scenes do not silently change.
- High-res 1×/2×/4× sun sigma agrees in world units; pixel mode scales as documented.
  The stock-equivalent branch agrees with baseline kernel within one output quantization
  step, while native AO is filtered once by its own denoiser.
- **Mutations:** retain the SSAO flag as the only native shadow-blur dispatch gate;
  AO accidentally filtered twice; shadow-only filter writes green; millimetres autoscaled
  twice. Each must fail its independence/scale gate. Review performance after removing
  redundant shared-green processing.

### P5 — Director controls, appearance presets, persistence and capture qualification

**Effect:** a searchable/favoritable native AO control with honest quality/view status,
saved creative looks and scene restoration; calibrated screenshot-only look presets.

**Files:** settings and native AO preset helper; `llfloaterdirector.cpp`,
`alkitpresets.cpp`, `alpresetlook.h`, `llpresetsmanager.cpp`; source generator
`scripts/code_tools/director_kit_gen.py`, `scripts/code_tools/director_kit/kit_pages.json`
and its actual page data providers; generated World Shadows XUI/index; Preferences and
Lightbox XUI/adapters as appropriate. Update relevant Director manifests/fixtures and
take-start metadata. Do not hand-edit only generated XUI/index and leave sources stale.

**Risk / effort:** medium (ownership, old scenes, Quick safety), **4–6 days**, plus
**3–6 days** of cross-scene/performance/look and hardware-capture motion qualification
shared with earlier phases.

Current search parses the v3/22-field generated index (`alkitsearch.cpp:67`, `:136`);
Quick proxies refer back to actual source rows (`alkitshell.cpp:2121`). Header Quick is
default deny, with listener tripwires, explicit per-setting review reasons and uniqueness
checks (`scripts/code_tools/director_kit_gen.py:1356`, `:1471`, `:1522`, `:1533`). New master is **not Quick-safe
just because it is a bool**: prove its listeners do not reload shaders/recreate global
buffers and that prepared master toggles allocate nothing. Unprepared requests must
report pending preparation instead of allocating from the toggle. Keep it denied until reviewed. Existing
`RenderDeferredSSAO` stays unsafe. Update generator count/fixtures only for real additions;
do not rename a nonexistent widget to make indexing pass.

Scene persistence is an explicit allowlist (`llfloaterdirector.cpp:3603`, `:4098`, `:4360`),
not every `Render*` setting. Existing Relight creative settings are included, while quality
is machine-wide (`:3734`). Look ownership intentionally excludes masters, quality,
targeting and shader-compile settings (`alkitpresets.cpp:61`). Follow these boundaries:

- AO look family owns radius, amount, thickness, chosen response and optional bent
  appearance controls; no master, feed enable, budget or quality changes on look load.
  Keep quality preset and appearance preset visibly distinct.
- Scenes explicitly save native master/algorithm and creative AO parameters. Proposed
  migration: a scene with no AO block selects stock compatibility, avoiding inherited
  native AO from the previously loaded scene. Show this policy in release notes; do not
  reset unrelated legacy SSAO/preferences or camera placement.
- Graphics preferences save quality/full-half/denoise and memory/work limits deliberately.
  Prepare is a separate machine preference default OFF; scenes/looks/graphics quality
  preset loads cannot turn it on or trigger preparation. Expose its pre-take cost/status
  and teardown explicitly. Scene load never raises machine ceilings; a scene requesting
  native while unprepared stays on stock and reports pending preparation. Take-start
  metadata records Prepare/prepared generation, requested and effective quality,
  per-preset sigma gate, seed, radius/thickness, pinned governor plan, view policy and fallback reasons, following
  `pipeline.cpp:26651` and `alrelighttakerecord.h:56`.

**Acceptance:**

- Search finds "AO", "ambient occlusion", "GTAO", "bitmask", "thin objects", "probe
  occlusion" and lands on a real widget. Row/header, reset, undo, changed-only state,
  Quick proxy and status agree. Existing generator/parity/functional checks pass.
- Save/load a creative preset without changing master, quality, budgets, subjects or
  feed enable. Scene roundtrip restores creative state; an old sparse scene after an
  AO-heavy scene returns to the documented stock state. Unknown keys are ignored safely.
  Custom quality numeric knobs roundtrip without preset listeners overwriting them.
- Toggle native master/algorithm/appearance through Preferences, Lightbox, Director and
  Quick after explicit preparation: reload count zero and zero allocation/release for
  prepared runtime toggles. Prepare remains excluded from Quick/scenes/looks and its
  default-OFF startup incurs no native allocation/link/cache cost.
  Explicit pre-take quality preparation allocates at most once per affected target;
  300 subsequent updates allocate zero. Reset works on actual declared settings.
- Match-look captures and 120-second frame-pacing/VRAM runs pass §9/§11. Publish requested
  versus effective state and any excluded alpha/tile/feed modes, not a misleading "Ultra".
- **Mutations:** add shader reload listener to an allowlisted native header; index a
  nonexistent widget; write master/quality from look load; omit new scene key; old scene
  retains previous native enable; status reports requested quality as effective after a
  budget clamp. Each must fail its specific ownership/UI oracle.

### P6 — Later optional bitmask SSGI

**Effect:** short-range diffuse bounce using visible newly covered sectors. Separate
master and strength, default OFF; AO remains usable without it.

**Files:** future source-radiance target and gather, bitmask utilities, diffuse-only
composition, target lifecycle, settings/UI/persistence and SSGI harnesses. Coordinate with
existing SSGI designs; do not reuse the final toned/graded/ReShade beauty as a light source.

**Risk / effort:** high, **8–15 additional days** after separate design/approval. This
estimate is less certain than P1–P5. Needs a current-frame scene-linear source with
explicit direct/emissive ownership, first-hit/sector ownership, albedo/energy controls,
and an account of existing probe irradiance. No feedback of the previous SSGI result,
no temporal history, no double-counted probes and no new direct-light AO modulation.

**Acceptance:** colored-wall bounce reaches only reference-visible nearby surfaces;
wall-occluded receiver remains at baseline within one RGBA8 step; duplicate/overlapping
samples cannot add energy twice; source intensity sweep is bounded/monotonic; SSGI OFF
is byte-identical to AO-only. Determinism, zero allocations, no reload and separate GPU
budget apply. **Mutations:** previous-frame source, already-composited bounce as source,
double sector ownership, final-beauty AO multiply and unoccluded color leaking through a
solid-wall fixture must fail. A detailed probe/SSGI energy oracle is required before coding.

**Estimated P1–P5 total: 40–60 engineer-days** including the shared qualification allowance;
roughly 8–12 working weeks for one engineer, about 1.4× the prior 28–44 day estimate.
P1 12–17 + P2 7–10 + P3 10–15 + P4 4–6 + P5 4–6 + shared 3–6 = 40–60.
This includes P1 light-map preparation/dispatch, motion qualification, separate legacy
probe design and combined admission; it is a planning estimate, not a commitment.
An initially useful P1+P2 main-view scalar implementation is about 19–27 days. Transparent
casters, padded tiled rendering and optional ReShade export are additional scope.

## 11. Screenshot-only "match the look" protocol

The user supplies captures of their modded iMMERSE **ON and OFF**, with the same camera,
scene, resolution, exposure, sky, materials and light-rig state. No proprietary file,
shader text, disassembly or implementation description is requested or inspected.
Reference images are user-provided visual targets only.

For each scene retain four labeled images: (A) viewer stock + external AO OFF,
(B) same viewer stock + user's reference effect ON, (C) native AO + external AO OFF,
(D) native AO debug visibility/bent normal. Also capture native master OFF to verify
baseline restoration. External non-AO grading may stay active only if identical in all
beauty comparisons. If the user's RTGI switch couples bounce and AO, record that and
compare the AO/contact component separately; P1–P5 cannot match bounce color changes.

Required scenes: indoor corner/under furniture; sunlit white wall with a nearby railing;
close avatar fingers/face/eyelashes/hair; alpha-mask fence and foliage; glossy PBR clothing
beside legacy glossy prims; nighttime rig with point/projector highlights; emission/Tron/
Roto silhouette; large populated region; 1/4/8 camera feeds; high-res and tiled stills.
Settle textures/probes and freeze world animation where feasible; take an ungraded
scene-linear diagnostic alongside display-referred screenshots.

Register image pairs and agree named ROIs: open wall, contact strip, finger gap,
background behind rail, hair edge, probe glint and direct-light highlight. Record mean
linear luminance ratios, contact width in pixels/metres, halo width and gap visibility.
Use a first calibration target of reference contact-ROI ratio within ±10%, contact width
within ±2 pixels at 1080p, without violating the stronger lit-gap/direct/emissive gates.
Do not optimize whole-image similarity by darkening sun or crushing the grade. Let the
user rank the final Natural / Defined / Reference-match appearance variants from paired
screenshots; quality remains an independent control. Freeze accepted values with image
IDs and settings so later shader changes can repeat the comparison.

## 12. Tier B decisions for the user

These choices are recorded for implementation, **not permission to make changes now**.
Recommendations supply a concrete starting design; unapproved alternatives are not
silently treated as agreed requirements.

| ID | Decision | Recommendation / consequence |
|---|---|---|
| B1 | Replace stock SSAO permanently or keep it selectable? | Keep Stock / Native Horizon / Native Bitmask during rollout, native master default OFF. Once qualified, bitmask can become the preferred choice with explicit migration. Retaining stock makes old scenes and OFF comparisons meaningful. |
| B2 | Default thickness and intended scene scale? | Start 3 cm, calibrate 1/3/10 cm screenshot sets. Keep Relight thickness separate. No automatic distance scaling initially. Walls versus lace cannot share a physically correct inferred thickness from one depth layer. |
| B3 | Should avatars under Actor Mask receive AO? | Yes, like other physical surfaces; Actor Mask remains local-light recipient selection. Offer a later receiver exemption only if desired; avatars/world still occlude each other. Flat Cover/silhouette/emission retain creative policy. |
| B4 | Expose native AO to ReShade? | Initially no new ABI texture; user disables external MXAO/RTGI AO while native is enabled. Later optional append-only ABI AO visibility/bent-normal semantics need generation/validity, own lifetime and consumer agreement. Never store it in material AO. |
| B5 | VCam coverage versus live performance? | Main only first; explicit real-feed opt-in after P3 isolation and combined budgets. Preview/media remain neutral. Feed status can report stock fallback, rather than silently sampling main AO. |
| B6 | Blended hair and alpha? | Keep current BLEND baseline; test the conservative receiver opt-in. Correct translucent casting is a separately scoped depth/coverage project. Alpha-tested geometry is included from P1. |
| B7 | Native ambient response: stock-compatible floor/HSV or physical visibility multiply? | Retain stock response for P1/P2 A/B; introduce a named native response/look only after screenshots. Neither can affect direct or emission; use exactly one diffuse response. |
| B8 | Old scene without AO keys: inherit current native AO or stock? | Select stock compatibility for old sparse scenes. Creative presets exclude master/quality; scenes own the master, machine owns cost ceilings. |
| B9 | Tiled stills: accept stock fallback initially or fund padded tiles? | Explicit whole-shot stock fallback initially. Add padded native tiles only with seam-free capture gates and a separate effort estimate. |
| B10 | Hardware and recording target for shipping qualification? | Declare primary GPU and 1080p60/4K30 or 4K60 requirement before tuning. Start High full at 1080p, Balanced half at 4K live; use measured headroom, not Intel's standalone timings, to choose the user's preset. |
| B11 | Permit relaxed Low/Balanced noise gates, or require sigma ≤0.005 in every advertised preset? | Recommend explicitly labeled Low ≤0.025 and Balanced ≤0.010, High/Cinematic ≤0.005, conditional on the CPU/GPU and common motion/detail gates. If every preset must reach 0.005, withdraw Low/Balanced until redesigned/rebudgeted; default live 4K currently has no modeled 0.005 preset under the 160 M gather ceiling. This is a quality/cost choice before implementation, not a passed qualification. |

No choice here permits proprietary code reuse, temporal AO accumulation or final-image
darkening. Those are fixed constraints.

## 13. Sun contact shadows alongside AO

After scalar AO is stable, a separate current-frame sun/moon contact visibility pass
can reuse `screenSpaceRayUtil.glsl` reconstruction/refinement concepts, tracing a short
distance toward `light_dir`. Combine validated contact visibility with native sun-map
visibility conservatively (for example `min(map, contact)`), keeping the shadow map's
offscreen coverage. Use its own enable, distance, bias/thickness, spatial filter and
budget; do not treat AO visibility as directional sun visibility. Hook the result into
light-map **red**, before its independent P4 sun filter, while AO remains in green.
It must not darken local lights or probe irradiance. Thin-chain/finger contact, map-only
parity at distance zero, offscreen fallback, camera-motion alignment and no double edge
are required gates, analogous to Relight contact checks. This is adjacent future work,
not included in the P1–P5 effort.

## 14. Principal risks and release conditions

| Risk | Specific mitigation / release condition |
|---|---|
| Halos / foreground AO bleeding | Correct finite intervals, edge/normal-aware filtering, actual sample coordinates, neutral invalid-neighbor resolve. Thin-gap and halo-width gates at full/half/mips. |
| Thick-object light leaks | Fixed thickness is an approximation, not geometry knowledge. Thick-wall reference fixtures and bounded radius; document leakage rather than hiding it with a final-image multiply. |
| Hair, alpha and missing backfaces | Separate OPAQUE/MASK/BLEND expectations, depth-safe receiver option, no transparent caster claim without additional depth representation. |
| Normal-map self-occlusion / bent artifacts | Separate macro and shading normals, bounded bias/hemisphere correction, cosine-weighted moment oracle and zero-moment fallback. |
| Probe/material/SSGI double counting | Keep authored AO, native visibility, probe influence and later bounce as distinct inputs. Probe-only specular hook before SSR replacement; never bake screen AO into probes. |
| Cost in large scenes / crowds | Pixel/sample ceiling bounds gather but not whole rendering/texture misses. Measure dense SL regions, alpha overdraw, probes, Relight and feeds together; enforce memory/filter ceilings and stable capture plans. |
| Large radius / depth mip loss | Physical short-range radius, projected cap, edge-LOD refinement, visibility masks tested against LOD0. Padded tiles or explicit fallback. |
| Shader portability / texture units | Raster-first GLSL, safe unsigned masks, native-only capability fallback, real links on NVIDIA/AMD/Intel as available; no unsupported-device "PASS" from a CPU-only model. |
| Toggle hitches / resource churn | Default-OFF explicit Prepare; static GL/extent support and latched admitted resources, never live governor-driven allocation predicates. Prepared light map/bundles retained across native OFF; white initialization and lighting/DOF ownership gates; zero allocation/reload toggles, 300-frame/120-second evidence and pool ownership tests. |
| Screen-locked noise / motion swimming without TAA | CPU covariance sizing before preset freeze; candidate denoise passes 3/4/7/8 and disclosed per-preset plane gates. Common stable-interior pan limits, contact-crease reference excess-delta gates and hardware-capture inspection; reject unqualified degraded presets or erased thin gaps. |
| Combined AO / Relight / VCam overcommit | Hard combined resident/transition-peak bytes and worst-frame work, main-first reservations and feed-first deterministic reduction; no timing-driven quality changes or per-frame eviction churn. |
| Creative looks regress | Direct/emissive/rim/Tron/silhouette invariants, shared Actor FX parity, screenshot ROIs, versioned scene migration and preserved stock path. |
| Overclaimed determinism | Equal-input AO readbacks across runs, frozen seed, no history; record whole-viewer input differences and never imply camera-only or cross-driver bit identity. |

Shipping evidence must include the named positive fixtures, rejected mutations, raw
readbacks, OFF baseline diffs, shader-reload/allocation counters, per-pass p50/p95/p99
timings and the user's paired look captures. This plan establishes that work; it does
not claim those implementation gates have already passed.

## User decision (10/21): qualification hardware

The only test machine is an RTX 5090.
- The hard caps are the hardware-independent work budgets (taps and iterations per frame, deterministic), which bind identically on every GPU.
- Millisecond targets are measured on the 5090 with a headroom factor for mid-range GPUs (assume about 3-4x slower; for example, Balanced at about 0.4 ms on the 5090 means about 1.5 ms on a 3060-class card).
- Cinematic may use more on the 5090.
- Other GPUs are checked from beta-tester Tracy captures and the logged work plan.
