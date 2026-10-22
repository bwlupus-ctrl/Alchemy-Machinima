# Relight world-space soft shadows and acne cleanup: plan

**Revision 2 (10/22).** Rewritten after three peer reviews of Revision 1
(`f1e8a9077b6`): Codex, Opus and GLM. All three found **no P0**. The changelog
below maps every review item to fixed, deferred or rejected.

**Planning only.** Nothing was implemented, built, committed or stashed. Source
was read with `git show` at `f1e8a9077b6` (code identical to `a483b9392f8`),
because another agent had an uncommitted planner change in the working tree.
That change landed as `84a3f731d1c` ("Thin objects: feeds keep soft shadows at
any budget") while this revision was being written. It touches
`alareashadowmath.h`, `pipeline.cpp`, settings and tests, so every `file:line`
below is an `f1e8a9077b6` line. Re-anchor against `84a3f731d1c` or later before
implementing anything, especially P0.1.

`NV/` = `indra/newview/`. `GLSL/` = `indra/newview/app_settings/shaders/`.
`.tmp/ref_private/` and all ReShade/iMMERSE files were not opened. The only
algorithm sources are public: PCF (Reeves 1987), PCSS (Fernando 2005; NVIDIA
"Integrating Realistic Soft Shadows into Your Game Engine", 2008),
normal-offset shadows (Holbert 2011), slope-scaled depth bias
(`glPolygonOffset`), receiver-plane depth bias (Isidoro 2006), the R2
low-discrepancy sequence (Roberts 2018), screen-space penumbra filtering
(Mohammadbagher et al. 2010), min-depth hierarchies for blocker search,
cube maps with guard bands, and buddy (power-of-two) allocation.

Statements marked **(inference)** or **(estimate)** need a fixture or an
in-world measurement before anyone relies on them.

---

## Changelog: Revision 1 to Revision 2

Reviewer IDs: **C** = Codex, **O** = Opus, **G** = GLM. "Fixed" means the plan
text now does what the review asked, or does something stronger, and the cited
section says how.

### Codex

| ID | Review item (severity) | Verdict | Where / evidence |
|---|---|---|---|
| C0 | Precision: the projector cutoff tests the **normal-offset receiver eye-space depth**, not radial distance; the sampler lightens before the hard cutoff (`shadowUtil.glsl:1092`). | Fixed | §1.1 row "Sampling range"; the authority function in §2.9 copies both terms. |
| C1 | P1: atlases don't hold the stated capacities (D32F = 4 B; 16 MiB holds two cubes; gutters break power-of-two). | Fixed | §2.3 recomputes usable versus allocated texels, gutters included. P1-lite uses a non-power-of-two fixed grid (13.55 MiB for two lights). Packing tests include gutters (§2.11.5). |
| C2 | P1: invalidation misses TEXANIM, probe-filtered texture verdicts, downscale stamps, `REBUILD_POSITION`, and the group `markRebuild` overload. | Fixed (design); implementation deferred to optional P5 | §2.11.1 caster-change coverage matrix. Deferred because no cache exists before P5, and P1-lite re-renders every frame. Hook-delivery tests (not injected events) are P5 gates. |
| C3 | P1: shadow recording needs its own lifecycle (`setRecording` called every update, `llreflectionmapmanager.cpp:278`; clear on off, `alprobedirty.cpp:307`; filters before record, `:221-231`). | Fixed (design); implementation deferred to P5 | §2.11.2 independent shadow subscriber with its own enable, reset, ring and drain. Gates cover shadows ON with probes OFF, probe resets, and probe toggles. |
| C4 | P1: raster admission makes visibility depend on history (warm vs cold cache). | Fixed | §2.10 and §2.11.3: admission reserves the **uncached worst case** for every resident light. Caching can only save time. It can never change residency, tier or visibility. Cold, warm, overflow and re-entry tests compare final visibility. |
| C5 | P1: face need and seams ignore the sampling footprint; a bilinear compare at a seam reads atlas neighbours. | Fixed | §2.4: every resident light renders **all six faces** every frame (no face-need culling). Each face is rendered with a widened FOV, so its gutter holds real depth in that face's own depth axis. Face selection happens per pixel, and the gutter covers the largest kernel. Edge and corner fixtures put unrelated tiles next to each face. |
| C6 | P1: PCSS radius units are wrong (`w·n/d_r` is a near-plane length); the 10-90% width of a disk is about 1.374 × radius. | Fixed | §2.6.2 defines every unit (axial depth versus radial distance, face Jacobian, projector FOV/aspect). The gate uses 1.374 × projected radius. Near-plane-invariance and off-axis tests are added. |
| C7 | P1: the contact cap doesn't cover the bias blind band (it classifies by distance along the biased ray; 0.15 m < 4 texels at 256/10 m; `min` keeps false-dark contact). | Fixed | §2.8 derives `d_c` per pixel from the map's actual bias terms and the contact origin offset. It defines what happens when the derived band exceeds the hard cap, and adds a false-contact fixture. P1-lite already uses a conservative derived band (§3, P1-lite). |
| C8 | P1: default native-slot reuse keeps the env-light distance hole. | Fixed | §2.9 per-receiver shadow authority: the gather weighs native validity per receiver. World coverage (or the full screen trace, in optional P0.6) returns beyond the native cutoff. Allocation, auction and count-before-filter are untouched. |
| C9 | P1: P0.1's floor breaks Thin objects' 48 near + far floor; 4 rays × 80 steps × 8 lights at half-res 4K is 5.31 billion; dropping channels can't help while the basis is requested K; `channels` means per draw. | Fixed | §3 P0.1 separates admitted lights `A`, draw width, the requested-K reservation and feed allowances. Thin's near 48 and far `minFar` are preserved. Light losses are published. The railing and join gates stay. |
| C10 | P1: horizon renormalization assumes sphere shading (off by default, `settings.xml:21647`); it mixes shading and geometric normals. | Fixed | §1.4 rank 3 is now a **hypothesis**. P0.4 is adopted only if it wins against independent lit-radiance references, in both Light shape states and with normal maps. The "terminator must read 255" oracle is gone. |
| C11 | P1: fixed-frame determinism doesn't prove moving-camera stability. | Fixed | §2.6.4 compares rotation candidates. §3 P2 has a moving-camera noise gate (same-world-point variance, periodic pattern, thin-blocker misses) at maximum supported softness. The P2 risk is raised to medium. |
| C12 | P2: prove the real rendering integration before trusting the schedule; uncached spike first; contact with the first world delivery; caching optional. | Fixed | New phase order (§3): P0, then the **P1-lite spike** (production casters, every frame, Tracy exit gate), then decision point D1. Minimum contact correctness is in P1-lite. Caching is the optional P5. |
| C13 | P2: face counts cannot promise ms. | Fixed | §2.10.3: runtime admission depends only on the preset and settings. The ms values are benchmark targets for named fixtures with fixed draw, triangle and skinned-mesh counts (§3, P1-lite exit gate). "Typical club scene" is gone. |
| C14 | P2: the governor's effective budget isn't static; oracles through the same filter hide blur; the byte-identical camera gate must be limited to unchanged geometry/LOD. | Fixed | §2.10.4: atlas admission uses static inputs only. §3 P0 gates add independent edge-location, contrast and penumbra-width oracles against the unfiltered analytic reference. Camera-motion byte gates now require frozen LOD (§1.6). |

### Opus

| ID | Review item (severity) | Verdict | Where / evidence |
|---|---|---|---|
| O1 | P1: PCSS can't work for large lights (search covers more than a face; about 170-texel filter; the cap is always hit, so the gate is tautological). | Fixed | §2.6.3: hierarchical min-depth search (cost doesn't depend on radius); search lower bound `z_s = max(n, r_L + 5 cm)`; map-side source clamp `r_map`; per-pixel PCF cap `R_pcf`; a **wide-penumbra hand-off** for the remainder (U16). The gates run at real rig sizes (0.3, 1, 2 m) and test both inside and outside the cap. |
| O2 | P1: the cache sink misses real changes (claims no `markRebuild` hook); VO-cache culling makes behind-camera blockers depend on the camera path. | Partly rejected; rest fixed | **Rejected with evidence:** `markRebuild` *is* hooked for every non-position rebuild flag (`NV/pipeline.cpp:5544-5550`). But the recorder drops tagged LOD and texture-animation churn, and the group overload and `REBUILD_POSITION` are unhooked (agrees with C2). **Fixed:** coverage matrix §2.11.1, including shape edits within unchanged bounds, material and alpha-mode changes, and same-bounds LOD swaps. VO-cache is a camera-dependent caster input (§1.6) with policy decision U15. Caching is out of the core. |
| O3 | P1: not camera-invariant; pops during live capture. | Fixed | §2.2: fixed tier per light, all six faces, no per-frame demotion. Residency changes only on listed events, always with a status line (U8: Pinned residency). §1.6 lists the remaining camera-dependent caster inputs. |
| O4 | P1: avatar-scene cost; `bucketShadowCull` passes bridges and individual drawables through unfiltered; use native projector slots as the cost anchor. | Fixed | §2.5: a new per-face AABB filter for drawables and bridges (the sun path is unchanged). The P1-lite exit gate measures the cost against two native projector slots in the same avatar fixture. |
| O5 | P2: under U3-A, the env-light symptom remains for mapped projectors. | Fixed | §2.9 (per-receiver authority); optional P0.6 fixes it for the screen trace now. U3-B is withdrawn. |
| O6 | P2: the filter is Gaussian, not a box; noise cells scale with `area_params.z`; a 4×4 interleave leaves a grid. | Fixed | P0.2 uses an R2 rank per noise cell with a UUID Cranley-Patterson shift. Its spectrum suits any low-pass kernel. A spectral-peak gate is added. |
| O7 | P2: 32 → 8 steps changes thin-blocker detection, contradicting "shapes don't change". | Fixed | The claim is removed (§3 P0, user-visible effect). HEAD already thins uniform steps to 1 under pressure (`alareashadowmath.h:139`). Rev 2 floors uniform steps at 8 and never thins Thin's near segment. |
| O8 | P2: U4 — a shade within 0.25 m is clipped by the 0.25 m near plane. | Fixed | §2.2: face near plane `n = 0.05 m`. The housing is excluded by prim identity (`RenderSpotLight`), not by the near plane. `RenderAreaShadowLightBias` stays a screen-trace setting. |
| O9 | P2: the guard band needs faces wider than 90°. | Fixed | §2.4 (93.47° faces, `g = N/32`). |
| O10 | P2: unbind sampler objects (`glBindSampler(u,0)`); one `CAMERA_LOCAL_SHADOW` shares `mVisible` stamps across lights (`llvieweroctree.cpp:761-775`). | Fixed | §2.5 camera IDs (one per resident light for a union cull, or one per face; the spike picks). §2.6.5 sampler hygiene, with a dispatch-fixture check that every unit's `GL_SAMPLER_BINDING` is 0 after the gather. |
| O11 | P2: the half-res world term inherits cause-6 stair steps. | Fixed | §2.1: the world term is resolved at **full resolution** in the upsample pass, and only the contact term is half-res. |
| O12 | Confirmations and recommended first deliverable (P0, then P1-lite, then a Tracy measurement before caching). | Adopted | §3 phase order and §3 effort summary. |

### GLM

| ID | Review item (severity) | Verdict | Where / evidence |
|---|---|---|---|
| G1 | P1: the "too slow" avatar exclusion depends on wall-clock time and breaks cache byte-equality. | Fixed differently | Avatars are always in the dynamic layer, re-rendered every frame (P1-lite has no cache at all), so a verdict flip can't leave a stale static tile. It is listed as an inherent input in §1.6 and in the matrix (§2.11.1). Making `isTooSlow` a pure heuristic is **rejected** as out of scope: it also drives the beauty pass (`lldrawpoolavatar.cpp:527-528`). |
| G2 | P1: the quadtree packing claim is asserted, not proven. | Fixed | P1-lite has no packing: it uses a fixed grid. Optional P5 states the buddy-allocation lemma with a proof sketch, uses tier-scaled gutters so tiles stay power-of-two in base units, and adds exhaustive pure tests (§2.11.5). |
| G3 | P1: the PCSS noise gate and cost aren't derived; 32 taps aren't in the ledger. | Fixed | §2.10.2: a separate world-resolve ledger term with the tap formula and a worked 1080p number. §3 P2 re-derives the noise gate per rotation candidate, at the capped kernel. |
| G4 | P1: how alpha-blend casters resolve into depth-only targets is unstated. | Fixed | §2.5 cites the mechanism: an `ALPHA_BLEND_CUTOFF` discard (`NV/pipeline.cpp:30238-30259`) in a depth-only pass with LESS, so the result is independent of order. The P1-lite native-equivalence fixture verifies it. |
| G5 | P2: effort optimistic. | Fixed | Every phase is re-estimated (§3 effort summary). The core grows from 21-31 to 26-38 days; the first deliverable is quoted separately. |
| G6 | P2: U3-B is closer to §10 than admitted. | Fixed (withdrawn) | U3-B is removed. Per-receiver authority (§2.9) changes only the gather's mix weight. Consumers stay byte-identical. |
| G7 | P2: the `shadow_clip.w` finding is an inference. | Kept | Still marked (inference) for the 10/21 link, with C0's precision added. |
| G8 | P2: P0.4 changes shadow shape; soften the claim. | Fixed | Same as O7. P0.4 is now conditional (C10). |
| G9 | P2: the contact march still uses half-res G-buffer depth; gate at half res too. | Fixed | P3 gates run at full and half trace resolution. Only the world term moved to full res (O11). |
| G10 | P2: the RTX 5090 numbers are targets, not measurements. | Kept | They are now benchmark targets tied to named fixtures (C13). |

---

## 0. Summary

**What the user gets, in order:**
1. **Phase 0:** today's screen-space soft shadows lose their grain and acne.
2. **P1-lite spike:** the top 1-2 point lights get real world-space shadow maps
   (six faces, every frame, like SL projectors but omnidirectional). They catch
   blockers that are off-screen, behind the camera or hidden, and they don't
   swim when the camera moves. We then measure what that really costs in an
   avatar scene.
3. **Decision point D1.** With the numbers in hand, the user decides how far to
   go.
4. **Later:** PCSS softness, a contact hand-off, projectors, optional caching,
   and VCam/UI.

**Top acne/grain causes (section 1.4):**
1. The work planner removes rays first: 1 ray per pixel at 4K Cinematic, and
   1-2 at 4K with Thin objects.
2. The bilateral filter is small and stops working on normal-mapped and grazing
   receivers.
3. **(hypothesis)** On curved receivers, disk rays below the surface horizon
   count as blocked.
4. Noise seeds are white per cell, the worst case for a small Gaussian.
5. The receiver offset and hit refinement use the normal-mapped G-buffer normal.

**Phases and effort (engineer-days):**

| Phase | What it delivers | Effort |
|---|---|---|
| P0 | Grain/acne fixes for current soft shadows (+ optional P0.6 distance-hole fix) | 6-9 (+0.5-1) |
| **P1-lite** | Uncached cube-map spike: top 1-2 point lights, fixed 512, PCF, Tracy exit gate | 9-13 |
| D1 | Decision point (user, with measured numbers) | 0.5 |
| P2 | PCSS with corrected units, wide-penumbra hand-off, moving-camera gate | 7-10 |
| P3 | Contact hand-off derived from the bias geometry | 4-6 |
| P4 | Projector world tiles and per-receiver native authority | 5-7 |
| P5 (optional) | Static/dynamic caching with the coverage matrix | 12-18 |
| P6 | VCam feeds, snapshots, UI, takes | 6-9 |

- **First deliverable** (P0 + P1-lite): **15-22** days (16-23 with P0.6).
- **Core** (P0 + P1-lite + D1 + P2 + P3): **27-39** days.
- **Full without caching** (core + P4 + P6): **38-55** days.
- **Full with caching:** **50-73** days.

**Decisions needed now** (section 4): U1 (grain or fewer lights, with
numbers), U3 and U13 (native-slot authority, and whether P0.6 ships with P0), U8
(Pinned residency), U14 (crowded scenes: keep lights and drop FPS?), U15
(VO-cache exemption). The rest can wait until D1.

---

## 1. Dive

### 1.1 Native projector shadow maps today

| Aspect | What HEAD does | Anchor |
|---|---|---|
| Allocation | Depth-only targets, `allocate(w,h,0,true)`, 24-bit depth by default. Size is screen width × `RenderShadowResolutionScale`, or `BDMergeProjectorShadowResolution` (256-8192). The slot count is `BDMergeMaxSpotShadows`; the hardware ceiling is 6/8/10 by texture units. | `NV/pipeline.cpp:2731-2762`; `NV/pipeline.h:1519-1545`; `indra/llrender/llrendertarget.cpp:140-145` |
| Compare state | `GL_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`, anisotropic filter, clamp, all set on the **texture**, not a sampler object. | `NV/pipeline.cpp:2794-2808` |
| Slot auction (off-limits, §10) | `setupSpotLight` bubbles priority: rig tier, then world, then actor-only. A Cast-shadows opt-out skips it. Fades use `gFrameIntervalSeconds` × camera velocity, so they depend on time. | `NV/pipeline.cpp:29248-29310`, `:31872`, `:31966-31975` |
| Map generation | Every frame, for every held slot: build the optical origin, near = apex + 0.01, far = apex + 1.5 × radius, then `updateCull` + `stateSort` + draw. Each slot has its own camera ID (`CAMERA_SPOT_SHADOW0+i`). **No caching.** The light's own prim is excluded (`RenderSpotLight`). | `NV/pipeline.cpp:32020-32136` (`:32126`); `NV/llviewercamera.h:54-63`; `NV/pipeline.cpp:5717` |
| Shadow pass content | Static and rigged passes, the avatar pools, alpha-mask, fullbright-mask, GLTF mask, grass, and alpha blend through a discard at `ALPHA_BLEND_CUTOFF` in a depth-only pass with LESS. Back-face culling on, occlusion culling off. | `NV/pipeline.cpp:30040-30330` (`:30095`, `:30238-30259`); `NV/pipeline.h:816-822` |
| Avatars | No shadows from impostored, jelly-dolled, invisible, muted or "too slow" avatars (Actor FX bypasses impostor and too-slow). `RenderAvatarShadowDetail` can drop passes. | `NV/lldrawpoolavatar.cpp:480-547` (`:512-528`) |
| Bias | Receiver offset `spot_shadow_offset` (0.04 m) along the G-buffer normal. Depth bias `0.8 × RenderSpotShadowBias` in post-projective units, so the bias in metres grows with distance². | `shadowUtil.glsl:1023`, `:454`; `settings.xml:20482-20503` |
| Filtering | Classic 5-tap, or "PCSS-lite" (a compare-sampler gap estimate), with 12 Poisson/Vogel taps and screen-coordinate rotation. | `shadowUtil.glsl:160-184`, `:450-535` (`:485`) |
| Sampling range (corrected, C0) | Tested on the eye-space depth of the **normal-offset** receiver: `spos.z > -shadow_clip.w`, else lit. Inside that range, line 1092 adds a lightening ramp from the midpoint of `shadow_clip.z..w` to fully lit at `shadow_clip.w`. So native shadows fade, then vanish, with distance from the camera. | `shadowUtil.glsl:1023-1100` (`:1026`, `:1092`, `:1099`); `NV/pipeline.cpp:25142` |
| Sun world-units path (reusable) | Texel-scaled receiver offset along N; exact map-space receiver-plane Jacobian with per-tap plane depth, slope cap and mm floor; perspective and ortho. | `shadowUtil.glsl:272-440` |

**Which costs dominate:** CPU: one cull + `stateSort` (which can rebuild
meshes, `NV/pipeline.cpp:5597`) + submission per map per frame; rigged draws in
a second pass. GPU: depth raster including per-map re-skinning and alpha-mask
fetches. Prism feeds repeat it (`:32199+`). Sun cascades can share one cull
(`RenderShadowCullMode 1`, `bucketShadowCull`, `:30640-30675`); projectors
don't.

**Finding outside this task (inference, verify in-world):** because of the
cutoff, a projector **holding** a native slot casts no map shadow on receivers
beyond the sun's far split, and the step-2 settled gather is contact-only
(`NV/pipeline.cpp:26847-26849`, `:27037-27046`). So a settled, mapped projector
shows **no shadow** on far receivers. This fits hypothesis 3 of the 10/21
"Env lights regression" note. §2.9 fixes it without touching the auction.

### 1.2 Can the same machinery render cube faces?

**Yes, with local changes**, but Rev 1 understated them (C12, O4, O10).

- `renderShadow(view, proj, LLCamera&, LLCullResult&, ...)` is generic over the
  matrices (`NV/pipeline.cpp:30040`). A cube face is a perspective camera with
  aspect 1. The probe code defines the six look/up vectors
  (`NV/llviewerwindow.cpp:6500-6517`).
- **Culling.** `bucketShadowCull` filters groups by frustum but **passes
  individual drawables and bridges through unfiltered** (`:30664-30674`). That
  is fine for sun cascades and wrong for six faces: every attachment and
  animesh would draw into every face. A face-filtered variant is needed (§2.5).
  It is not a static/dynamic splitter.
- **Camera IDs.** `isVisible()` and `setVisible()` stamp `mVisible[sCurCameraID]`
  with the frame number (`NV/llvieweroctree.cpp:761-775`). Two culls
  in one frame under one ID corrupt each other. That means one ID per
  independent cull (§2.5).
- **Per-face `stateSort`** (`:30105`) runs on every face.
- **Atlas target:** `LLRenderTarget` with `DEPTH_FMT_32F`, which is **4 bytes
  per texel** (`llrendertarget.cpp:140-142`).

Gaps to close (unchanged from Rev 1, still true):
1. A tile viewport + scissor + scissored depth clear.
2. `DEFERRED_SHADOW_TARGET_WIDTH` must be the tile width (`:30219`, `:30233`).
3. Fixed-size `LLCullResult` storage per cull (precedent `:32124`).
4. Raw depth for the PCSS search: a non-compare sampler object (P2 only).

**Cube layout:** six faces as atlas tiles (chosen). Rejected: dual-paraboloid
(needs tessellation, and large flat SL prims rasterize wrongly) and tetrahedral
(awkward tiles). Layered `gl_Layer` rendering comes later (P8).

### 1.3 Probe cube faces and the event recorder

- `cubeSnapshot` renders a full deferred frame per face
  (`NV/llviewerwindow.cpp:6420-6530`, `NV/llviewerdisplay.cpp:1426-1531`). It
  gives us orientation conventions only, not a cost model.
- `ALProbeDirty` (`NV/alprobedirty.h:39-206`) has hooks for xform deltas,
  insert/remove, light volume, texture arrival/downscale, LOD tags
  (`alprobedirty.h:62-70`) and region shift. But:
  - recording is set **every update** from probe state
    (`NV/llreflectionmapmanager.cpp:276-278`), and turning it off clears
    everything (`alprobedirty.cpp:301-311`);
  - events are classified, provenance-filtered (`TAG_VOCACHE`) and tag-dropped
    **before** they are recorded (`:215-234`);
  - texture arrivals go through a probe-resolution verdict (`:525-543`);
  - the drain debounces on wall-clock time.
- So a shadow cache can't simply share it. It needs an independent subscriber
  (§2.11.2). This only matters for optional P5.

### 1.4 Where today's grain and acne come from (ranked)

| Rank | Cause | Evidence | Where it shows |
|---|---|---|---|
| 1 | **Ray starvation by the planner.** `workloadWithin` removes rays down to 1 **before** steps or resolution (`NV/alareashadowmath.h:137`; uniform steps after, `:139`). Cinematic asks for full res with 24 rays (`:70`). | HEAD tables (`RELIGHT_SL_INTEGRATION.md:1627-1665`): 4K Cinematic = 1 ray at K4 and K8; Thin ON at 4K K8 = 1 ray for every preset. With S rays the binary estimate's std is `sqrt(p(1-p)/S)`: 0.5 at S=1, 0.18 at S=8. | Every penumbra at 4K and Cinematic; worse at K=8 and with Thin. |
| 2 | **The filter is small and its edge-stops collapse.** Radius = `denoise(2) × H/1080 × (half ? 0.5 : 1)`, so 1 trace pixel at 1080p half-res (`NV/pipeline.cpp:27014-27016`). Weights are Gaussian `exp(-t²/r²)` (`areaShadowFilterF.glsl:37`) times `pow(dot(n,n'),32)` on normal-mapped normals and `exp(-dz/max(0.03,0.005z))` on raw depth (`:12-16`). A 15° bump gives a weight of 0.33, 25° gives 0.04. | Code | Speckle on brick, stone and fabric normal maps, distant floors, grazing walls. |
| 3 | **(Hypothesis, C10)** Below-horizon disk samples count as blocked on curved receivers. | `areaShadowF.glsl:54-75`; `screenSpaceRayUtil.glsl:107-129`. Sphere shading integrates the horizon (`deferredUtil.glsl:935-956`), but Light shape is **off by default** (`settings.xml:21647-21652`). That code uses the shading normal; the fix would use the geometric one, and the two can legitimately disagree on normal-mapped surfaces. A finite disk physically straddles the terminator (`RELIGHT_SL_INTEGRATION.md:1698-1701`). | Grainy terminator band on avatars and round props. **Size and correct fix unknown** until P0.4's radiance references. |
| 4 | **White-noise seeds per noise cell.** Cells are `gl_FragCoord / area_params.z` (`areaShadowF.glsl:39`), so at 4K full res a cell is 2×2 pixels. Each cell hashes its own rotation and jitter (`:47-49`). | Code | Amplifies 1 and 2. |
| 5 | **Offset and refinement use the shaded normal** (`areaShadowF.glsl:37`; `screenSpaceRayUtil.glsl:30-45`, `:112-125`). 0.35 m thickness acceptance. | Code; magnitude is an inference | Specks on normal-mapped flat receivers and creases. |
| 6 | **Half-res upsample at edges** (`areaShadowFilterF.glsl:44-62`). | Code | Stair steps at silhouettes in half-res presets. |
| 7 | **Ruled out:** depth precision and RGBA8 (Rev 1 arithmetic stands). | | |

### 1.5 What to reuse

| Asset | Reuse | Anchor |
|---|---|---|
| Selection and ranking | The same pure selector ranks residency candidates. | `NV/pipeline.cpp:26825-26858`; `alareashadowmath.h:24-54` |
| Light size | `area_source_radius` (fixture, Size from prim, World light size, 0.10 m fallback). | `NV/pipeline.cpp:11784-11803`, `:27048` |
| Planner | Extended in P0 (`A` admission, a ladder). World taps get their own term (§2.10). | `alareashadowmath.h:76-198` |
| Visibility channels and consumers | World visibility lands in the existing `areaVisibility` planes. Consumers stay byte-unchanged (`min` for projectors, multiply for points). | `spotLightF.glsl:196-197`; `pointLightF.glsl:298-299`; `multiPointLightF.glsl:169-170`, `:226-227` |
| Contact classification | `areaTrace` returns full and contact visibility from one march, classified by `hit_t` along the ray from the biased origin. | `screenSpaceRayUtil.glsl:46-133` (`:128`) |
| Thin objects | Kept for screen-only lights. | `screenSpaceRayUtil.glsl:55-88`; `alareashadowmath.h:60-88` |
| Geometric normal from depth | The `rigRimAuxF` method. | `GLSL/class1/deferred/rigRimAuxF.glsl:124-150` |
| Sun receiver-plane bias | Port `sunBandC`/`pcfShadowC`. | `shadowUtil.glsl:272-440` |
| Native optical origin | One shared helper for projector tiles (P4), native matrices byte-identical. | `NV/pipeline.cpp:32050-32101` |
| View policy, Director card, scene save, takes, VCam feeds, harnesses | Same as Rev 1. | `NV/pipeline.cpp:26612-26622`; `panel_dpage_light_rim.xml:234-295`; `llfloaterdirector.cpp:3743-3744`; `alrelighttakerecord.h:11-48`; `NV/pipeline.cpp:11222`, `:11521`, `:11686`; `scripts/code_tools/relight_*` |

### 1.6 Camera-dependent caster inputs (new, O3, C14, G1)

Even a world-space map renders whatever the viewer currently has loaded. These
inputs depend on the camera or on time. They affect native SL projectors too.

| Input | Mechanism | Policy |
|---|---|---|
| Volume/mesh LOD | Camera distance | U12: camera LOD in v1. Byte-identical camera gates run with LOD frozen. |
| Avatar impostor, jelly doll, too slow | Camera distance, avatar count, frame time (`lldrawpoolavatar.cpp:512-528`) | Inherent; same as native. Avatars are always re-rendered, so there is no cache staleness. |
| VO-cache rear culling | `killInvisibleObjects` deletes objects out of view over time (`NV/llviewerregion.cpp:1782-1800`) | **U15.** P1-lite counts resident-sphere casters that this cull removes. |
| Texture discard on alpha casters | Camera-driven priority | Inherent; cut-outs coarsen with distance. |
| Interest list | The server sends objects by camera | Inherent (B9). |

The P1-lite status line shows how many of these changed this frame, so a pop in
a shot can be explained.

---

## 2. Target architecture

### 2.1 Frame flow

```
frame start (before LLPrismLens::renderAuxiliaryView, llviewerdisplay.cpp:944)
  WorldShadow::plan()    pure: resident set (rules §2.2), fixed tiers,
                         fixed grid slots, no timing or cost input
  WorldShadow::render()  for each resident light: cull, then 6 faces at the fixed
                         tier, widened FOV, per-face filtered casters
                         (P5 only: skip valid static tiles + draw the dynamic layer)
per view (Prism feed / main), inside renderAreaShadows (pipeline.cpp:27055):
  gather (trace res):    world-owned light -> short contact march only (§2.8)
                         screen-only light  -> today's trace (P0 arithmetic)
                         mapped projector   -> authority weight α(z) (§2.9)
  H/V bilateral (contact/screen terms only)
  upsample (full res):   screen/contact upsample, then for world-owned lights
                         vis = min(world_fullres, contact_upsampled)
  consumers: UNCHANGED (point × vis, projector min(native, vis))
```

The world term is never blurred by the screen filter and never upsampled (O11).
In the PCSS phase, its wide-penumbra residue is the one exception (§2.6.3).

### 2.2 Which lights get maps; residency and camera invariance

- **P1-lite:** point lights only, `RenderWorldShadowLights` = 1 or 2 (default
  1).
- **Fixed tier per light.** The tier comes from the preset and settings only:
  P1-lite is 512; later Medium 512, High 512 / rank-1 1024, Cinematic 1024.
  There is no screen-size tier, so a dolly never re-tiers a light (replaces
  Rev 1's U8 tier freeze).
- **All six faces, every frame**, for a resident light. There's no face-need
  set, so turning the camera never changes which faces exist (C5, O3).
- **Near plane** `n = 0.05 m`, far = 1.5 × radius. The housing is excluded by
  prim identity (`RenderSpotLight`, `NV/pipeline.cpp:5717`; whole linkset per
  U4). A lamp shade 0.1 m from the bulb still casts (O8). D32F at n = 0.05,
  z = 20 m resolves about 0.5 mm.
- **Residency** (who owns a map). The candidates are the existing selector's
  ranked set (camera-dependent, `NV/pipeline.cpp:26825-26858`). Two modes (U8):
  - **Auto:** resident = the top R selected point lights each frame. A change
    is allowed, but always shows a status line `World shadows: lamp_3 → screen
    (rank change)`, a `[RelightQ]` log line and a take-record field.
  - **Pinned** (recommended during shots): the UUIDs resident when Pin is
    pressed stay resident until Re-pick, until the light is deleted or turned
    off, or until it leaves draw distance (each shows a status line). A pinned
    light stays resident even when it is off-screen, because its map is
    world-space.
- **No per-frame demotion.** Nothing in the frame loop demotes a resident light
  for cost (§2.10.3). In a crowded scene the frame rate drops instead (U14).
- Projectors stay on native slots or the screen trace until P4. Lights that
  aren't resident keep the screen trace, so world shadows are never
  all-or-nothing.

### 2.3 Atlas layout and byte math (C1)

`N` = usable face texels, `g` = gutter per side, tile `T = N + 2g`. D32F =
4 B/texel.

| Item | P1-lite and P2 (`g = N/32`) |
|---|---|
| Face 512, tile 544 | 544² × 4 B = **1.129 MiB** |
| One point light (6 tiles) | **6.77 MiB** |
| Fixed grid, 1 light: 3 × 2 tiles = 1632 × 1088 | **6.77 MiB** |
| Fixed grid, 2 lights: 3 × 4 tiles = 1632 × 2176 | **13.55 MiB** |
| 4 lights at 512 (after D1): 6 × 4 = 3264 × 2176 | 27.09 MiB |
| Face 1024, tile 1088: one light | 27.09 MiB |
| PCSS min-depth pyramid (R32F, levels 1-5, P2) | about 1/3 of base: +4.5 MiB for 2 lights at 512 |

- The atlas is **not** power-of-two. `LLRenderTarget::allocate(w,h)` takes
  arbitrary sizes. The grid is fixed, so there is no packing and no tile moves
  in P1-lite through P4.
- `g = N/32` (16 at 512) is chosen up front so that P2's kernels fit without a
  re-layout. P1-lite needs only 4. The cost is 12.9% memory overhead.
- Tile origins are multiples of 544, so the min-depth pyramid stays
  tile-aligned to level 5 (544 / 32 = 17).
- Rev 1's "16 MiB 2048² holds 4 lights" was wrong: 2048² holds 16 unpadded
  512² faces (2.67 lights). The P5 numbers are redone in §2.11.5.

### 2.4 Seams and face selection (C5, O9)

- Each face is rendered with a **widened FOV** `θ = 2·atan(T/N)` = **93.47°**
  for `g = N/32`. The usable 512² interior then maps exactly to the 90° face.
  The gutter holds **real depth for that face's own projection**, so no
  neighbouring-face transform or copy is needed.
- **Face selection happens per pixel** from the receiver direction (major
  axis). The whole kernel is evaluated in that face's tile. The gutter covers
  the kernel as long as `R_pcf + 1 (bilinear) + 1 (receiver-plane slope) ≤ g`.
  At 512 that allows R_pcf ≤ 14 texels.
- Rev 1's per-tap face switch is dropped, because the gutter makes it
  unnecessary. Each tile has its own scissor, so a neighbouring tile is never
  read.
- **Seam fixtures:** a blocker straddling every edge and every corner. The tiles
  next to each face are filled with unrelated depth (cleared, near-zero, and
  another light's faces), so reading past the gutter fails visibly.

### 2.5 What the shadow pass contains

- **Same content as native `renderShadow`:** static, rigged and avatar pools,
  animesh, alpha-mask, fullbright-mask, GLTF mask, grass, and alpha blend
  through the `ALPHA_BLEND_CUTOFF` discard (`NV/pipeline.cpp:30238-30259`). In a
  depth-only LESS pass, the result doesn't depend on draw order (G4).
  `RenderAvatarShadowDetail` and the avatar exclusions apply unchanged. No
  particles. Back faces culled (U5). `glPolygonOffset` for atlas draws only.
- **Cull** (the spike measures both):
  - **A:** six `updateCull` + `stateSort` per light, like native. Needs six
    camera IDs per light: `CAMERA_LOCAL_SHADOW0..11` for 2 lights, appended
    before `NUM_CAMERAS`.
  - **B:** one union cull per light using an `LLCamera` whose six agent planes
    bound the light's sphere AABB (inference: verify `LLCamera` accepts
    hand-built planes). Then per face, a filtered bucket. Needs one ID per
    resident light.
- **New `bucketShadowCullFace`.** Like `bucketShadowCull`, but individual
  drawables and bridges are also filtered: a drawable by its extents, a bridge
  by its spatial extents, against the face frustum (O4). The existing sun
  function is untouched.
- **State hygiene:** viewport, scissor, depth func, polygon offset, colour mask,
  bound FBO, texture units, shader and (P2) sampler bindings are restored. A
  debug self-check snapshots GL state before and after `WorldShadow::render()`
  and logs `[RelightQ] state-restore FAIL <item>`.

### 2.6 Filtering

#### 2.6.1 P1-lite: PCF with a clamped light size

- 16 hardware-bilinear compare taps on a Vogel disk of radius `ρ` texels:
  - `ρ = clamp(0.5 · r_Lc / texel_world(z_r), 1, 3)`;
  - `r_Lc = min(r_L, 0.10 m)`;
  - `texel_world(z) = 2z/N`.

  The 0.5 factor is the PCSS width for a blocker at 2/3 of the receiver
  distance: a fixed-geometry approximation. Real contact hardening is P2.
- Compare state is set on the atlas texture, like native, so P1-lite needs no
  sampler object.
- Rotation: shadow-texel-anchored (§2.6.4 candidate b), keyed by the light
  UUID.

#### 2.6.2 Units (C6)

For a face with axis `a` and light centre `C`, the receiver `P` has **axial
depth** `z_r = dot(P - C, a)` and **radial distance** `d_r = |P - C|`. The face
direction cosine is `cos φ = z_r / d_r`. For a 90° face, usable UV =
`0.5 + 0.5 · (x/z, y/z)`, so a lateral world length `ℓ` (perpendicular to `a`)
at axial depth `z` spans `ℓ · N / (2z)` usable texels.

- **Fernando's similar triangles use axial depths** (planes parallel to the
  near plane): blocker search radius in UV at depth `z_s`:
  `R_s = r · (z_r - z_s) / (2 · z_s · z_r)`. Penumbra radius on the receiver
  plane: `w = r · (z_r - z_b) / z_b`. Filter radius in UV: **`w / (2 z_r)`**
  (Rev 1's `w·n/d_r` is a near-plane length; C6).
- **Off-axis.** An SL light is a sphere, whose disk faces the receiver. On the
  face-parallel plane its extent is `r / cos φ` along the radial-in-face axis
  and `r` along the tangential axis. The PCF disk is therefore scaled
  anisotropically by the face Jacobian at the receiver direction. That is an
  ellipse, at no extra tap cost.
- **Projectors (P4):** UV per metre = `1 / (2 z tan(fov_x/2))` and
  `1 / (2 z tan(fov_y/2))` with the native aspect from the shared helper.
- **Gate oracle:** a disk-over-straight-edge profile is
  `f(s) = (acos s - s·sqrt(1-s²)) / π`, so the 10-90% width is
  **1.374 × the projected radius** (solve `f(s) = 0.1`: `s = 0.687`).
- **Near-plane invariance:** changing `n` from 0.03 to 0.10 m changes no
  penumbra (n enters only through `z_s` and depth precision).

#### 2.6.3 P2: PCSS for SL-sized lights (O1)

At rig sizes, a blocker near the light can shadow most of a face. That's
physics, not a bug, so the search must not depend on tap count:

1. **Search lower bound** `z_s = max(n, r_map + 0.05 m)`. Blockers inside the
   fixture volume are the fixture.
2. **Map-side source clamp** `r_map = min(r_L, R_map_max)`, default 0.25 m
   (U16).
3. **Hierarchical blocker search** on a per-tile **min-depth pyramid** (R32F,
   levels 1-5, built after each face render by 5 reduction passes):
   - pick level `L = clamp(ceil(log2(R_s·N / 4)), 0, 5)`;
   - take 4 `textureGather`s (16 texels) there for "any blocker" and `z_min`;
   - if there are blockers, take 16 taps at level `max(L-2, 0)` and average
     the non-lit texels to get `z_b`;
   - no blockers means lit (early out).

   Cost doesn't depend on radius.
4. **PCF** with `w = r_map · (z_r - z_b) / z_b`, radius in texels
   `w · N / (2 z_r)`, anisotropic as in §2.6.2, **capped at `R_pcf`** (Medium
   and High 8, Cinematic 12) with 16 or 32 bilinear taps. The cap now binds at
   real sizes, and the gates test both sides of it (no tautology).
5. **Wide-penumbra hand-off** for the part the map doesn't produce:
   `w_true = r_L (z_r - z_b) / z_b` versus the PCF width actually used. Two
   options (U16):
   - **Clamp only:** big fixtures cast sharper world shadows than their true
     size. Deterministic and free.
   - **Screen-space penumbra blur** (Mohammadbagher 2010): the residual radius
     `w_res = sqrt(max(w_true² - w_used², 0))` (Gaussian composition,
     (estimate), gated within 20%) is converted to pixels at the receiver's
     eye depth. The world term gets a separable bilateral blur of up to 16
     full-res pixels per axis, with the P0.3 plane-distance and geometric-normal
     weights. It is world-sized, so it is camera-invariant to first order. It
     clamps at screen borders: documented, gated as no visible seam ≥ 3/255
     along the border.
6. **Contact hardening:** `z_b → z_r` drives `w → 0`. A zero `r_L` uses the
   existing 0.10 m shadow-only radius.

#### 2.6.4 Rotation and moving-camera stability (C11)

There are three candidates, all deterministic. P2 picks one by gates, not by
taste:
- **(a)** screen-cell rotation (HEAD style, `shadowUtil.glsl:485`; it changes
  when a receiver crosses a cell);
- **(b)** a **shadow-texel-anchored** rotation: an R2 rank of the receiver's
  face texel index plus the UUID. The same world point gets the same pattern
  from any camera;
- **(c)** a fixed disk with no rotation (stable, but banding in wide
  penumbrae).

Expected winner: (b). Its risk is a texel-scale pattern under magnification,
which the spectral gate measures.

#### 2.6.5 Sampler hygiene (O10)

- P2's raw-depth reads use a non-compare **sampler object** on the gather's
  units. The `LLTexUnit` cache doesn't know sampler objects, so every
  `glBindSampler(u, s)` is paired with `glBindSampler(u, 0)` before the program
  unbinds.
- The dispatch fixture asserts `GL_SAMPLER_BINDING == 0` on all units after the
  gather and after the upsample.

### 2.7 Acne prevention and tuning

Per tap, in this order:
1. Normal offset along the **geometric** normal `N_g`:
   `o_n = min(k_n · texel_world(z) · sin θ, 2 texels, 5 cm)`.
2. Receiver-plane depth bias: the exact perspective Jacobian (`sunBandC`,
   `shadowUtil.glsl:398-438`) on `N_g`, slope cap 8.
3. Constant floor `b_c` (mm).
4. Raster slope bias `glPolygonOffset(f_slope, u)`, capped through
   `glPolygonOffsetClamp` where available; otherwise `f_slope` is limited by the
   P3 band constraint.

The tuning procedure is unchanged from Rev 1: zero biases must fail the
fixtures, then raise `k_n`, `f_slope` and `b_c` in steps, back off on
peter-panning, and check the 5 cm leak wall. One new constraint: **the blind
band `B(P)` of §2.8 must stay below `d_hard` everywhere in range** at that tier.

Acne gate as Rev 1: at least 99.9% of unoccluded pixels with `N_g·L > 0.05`
read ≥ 254/255, and no component of 3 or more pixels reads below 250.

### 2.8 Contact hand-off from the actual bias geometry (C7)

**The map's blind band.** A blocker at ray distance `t` from receiver `P`
toward the light is invisible to the map when
`t < B(P) = o_n·cos θ + b_c + Δ_slope,max + ε_rp`. These are the per-tap terms
of §2.7, evaluated in the gather with the same inputs, in metres along the ray.

**The contact trace's coordinate.** It marches from `O_s = P + N_g·b_s`, where
`b_s = max(2 cm, 1.5 · footprint)` (`screenSpaceRayUtil.glsl:38-45`). It
classifies by `hit_t` from `O_s` (`:115`, `:128`). A blocker at `t` is hit at
about `t - b_s·cos θ`. The lateral shift `b_s·sin θ` is covered by the grazing
fixtures.

**Hand-off per pixel:**

```
d_req(P) = B(P) - b_s·cos θ + δ        δ = one contact step spacing (overlap)
d_c(P)   = clamp(max(d_req(P), user_extra), 2 cm, d_hard)
```

- `user_extra` is `RenderAreaShadowMappedContact` (default 0.15 m). For
  world-owned lights it now means **extra** contact detail, never less than
  required.
- `d_hard = 0.5 m` (estimate). Beyond it the screen trace gives
  camera-dependent displaced edges.
- If `d_req > d_hard`, the tier can't cover that receiver. The pixel is counted
  in the status line and debug view. The gate requires **zero** such pixels
  within range at default biases for each fixed tier, which is why §2.7 tuning
  respects `B(P) ≤ d_hard`.
- **False-dark contact.** `min(world, contact)` keeps any false contact
  shadow, so world-owned contact uses a tightened thickness
  `min(thickness, max(4·footprint, 5 cm))` for every hit. A fixture puts a
  camera-side pole in front of the receiver on screen but off the light ray: it
  must not darken by more than 2/255.
- **P1-lite** uses a conservative CPU constant instead of the per-pixel form:
  `d_c = max(user, B_max(512, range))`, where `B_max` is the worst case of
  `B(P)` over the light's range. P3 replaces it with the per-pixel form.

### 2.9 Native projector slots: per-receiver authority (C8, O5)

- **Rule:** the native map is authoritative only where the native sampler
  actually shadows.
- **Native validity.** For receiver eye depth
  `z = -(pos + N·spot_shadow_offset).z`, with the same G-buffer normal
  `spotLightF` uses, let `t = (z - clip.z) / (clip.w - clip.z)`. Then
  `α(z) = 1 - clamp(2t - 1, 0, 1)` for `z < clip.w`, and `α = 0` beyond. This
  mirrors `shadowUtil.glsl:1023-1099`, including the `w` term (the exact copy
  is a P0.6 implementation detail).
  - It is duplicated in the gather. A static check (`relight_shader_check.py`)
    asserts both copies stay textually in sync, the same pattern as the
    existing KEEP IN SYNC dispatch.
  - `shadowUtil.glsl` and `spotLightF` are **not edited**.
- **Gather for a mapped projector:**
  `out = mix(full, contact, fade · α(z))`.
  - `full` is today's full-length screen visibility (optional P0.6), or
    `min(world, contact)` when the projector has a world tile (P4).
  - The consumer stays `min(native, out)`. Beyond the cutoff `native = 1` and
    `α = 0`, so the receiver gets real shadow. In the ramp, `min` never
    double-darkens, and continuity follows from `α` being continuous.
- **§10 untouched:** no change to `BDMergeMaxSpotShadows`, allocation, the
  auction, fades or count-before-filter (`NV/pipeline.cpp:26820`, `:29265`).
  U3-B (consumer skips native) is **withdrawn**.
- **Cost of P0.6 is about zero:** the gather already computes full and contact
  from one march (`areaShadowF.glsl:71-78`). It adds `shadow_clip` uniforms
  (verify whether `bindDeferredShader` already sets them, and what they are
  when sun shadows are off) and one multiply.

### 2.10 Budget, admission, memory

#### 2.10.1 Screen ledger

World-owned lights are charged to `RenderAreaShadowBudget` at their **contact
march** cost (`rays_c × steps_c`, about 4 × 8) instead of a full trace. So
enabling world shadows never makes screen-only lights grainier.

#### 2.10.2 World-resolve ledger (G3)

A separate pure term:
`taps = pixels_full × L_world × (search_taps + pcf_taps)`.

- P1-lite at 1080p with 2 lights: 2,073,600 × 2 × 16 = **66 M** compare taps.
- P2 Cinematic at 1080p: 2,073,600 × 2 × (8 gathers ≈ 32 texels + 32) ≈
  **265 M** texel reads.

Ceiling: `RenderWorldShadowTapBudget`, default 600 M. Over the ceiling, the tap
count steps down by preset (32 → 16 PCF taps) at plan time, never per frame by
cost.

#### 2.10.3 Raster admission: deterministic inputs only (C13, O3)

- Runtime admission is a pure function of settings: the resident count R
  (1-2 in P1-lite), the tier, and the faces (always 6).
- **No input from draw counts, triangle counts or ms.** Those depend on the
  scene and would make residency pop mid-shot.
- The **status line** reports, per frame: resident lights, faces, draws by pass
  class (static, mask, blend, rigged, bridge, GLTF), triangles, skinned meshes,
  culled-by-VO-cache count, and Tracy CPU/GPU ms.
- The ms numbers in the gates are **benchmark targets** on named fixtures with
  fixed counts (§3 P1-lite).

#### 2.10.4 Memory and the governor (C14)

- The atlas size is a fixed function of R and tier (§2.3): 6.77 or 13.55 MiB in
  P1-lite. It is always allocated on enable, shown in the status line, and
  counted by the governor (render targets are part of texture accounting,
  `NV/llviewertexture.cpp:654-663`).
- Admission of larger atlases (after D1 or in P5) compares against static
  inputs only: detected VRAM and the configured `max_vram_budget`. It never uses
  `sEffectiveVRAMBudgetMegabytes`, which moves with fixed allocations
  (`:663-677`).
- If the reservation doesn't fit, it fails closed to fewer resident lights at
  enable time, with a status line.

### 2.11 Caching (optional P5 only)

Caching is a pure cost optimization. It must never change what is drawn.

#### 2.11.1 Caster-change coverage matrix (C2, O2)

Every row must have a **delivered-hook** test in P5 (a real code path drives the
change, not an injected event).

| Change | Existing hook / status at HEAD | Shadow action |
|---|---|---|
| Xform move (bounds Δ > 1e-4 m) | `ProbeMoveNote` → `noteDrawableBoundsIfChanged` (`llspatialpartition.cpp:990-1011`) | Invalidate faces hitting old ∪ new AABB |
| Sub-threshold or bounds-preserving move (a rotation inside symmetric bounds, target omega) | `noteNoopMove`: **no event** | Classify spinning/omega objects as **dynamic** |
| Insert / remove | Recorded (`alprobedirty.h:162`) | Invalidate |
| VO-cache re-creation | Recorded but **dropped** as `TAG_VOCACHE` (`alprobedirty.cpp:225-229`) | Shadow subscriber takes it before the provenance filter: invalidate |
| VO-cache rear cull (object deleted) | Removal | Invalidate; U15 decides whether it may happen inside resident spheres |
| Shape-parameter edit inside the same bounds (cut, hollow) | `markRebuild` non-position flags **are** hooked (`NV/pipeline.cpp:5544-5550`) | Invalidate (verify the delivery path) |
| `REBUILD_POSITION` only | **Not hooked** (`:5547` masks it) | Add a shadow-only hook: invalidate |
| Group `markRebuild` overload | **Not hooked** (comment `:5546`) | Add a hook: invalidate the group AABB |
| LOD swap, same bounds | Tagged `TAG_LOD_*` (`alprobedirty.h:65-69`); dropped by the probe recorder | Shadow subscriber keeps it: invalidate |
| Texture animation | `Motion::TEXANIM` (`llvovolume.cpp:630`, `:638`); toggle tag | Animating alpha casters become **dynamic** |
| Texture arrival / discard change on alpha faces | Probe verdict filters by probe resolution (`alprobedirty.cpp:525-543`); downscale only stamps (`:604`) | Shadow subscriber: own verdict (any discard change on an alpha-mode face in the pass) → invalidate |
| Material / alpha-mode / cutoff / glTF material change | TE update (verify that material and alpha-mode edits reach it) | Invalidate; add a hook if missing |
| Media texture on an alpha face | none | **Dynamic** |
| Flexi prims, active physics | Moving bridge | **Dynamic** |
| Static → active transition (a prim starts moving) | Bridge change | Invalidate the static faces it was baked into, then dynamic |
| Link / unlink | Verify | Invalidate both AABBs |
| Avatars, attachments, animesh, impostor / too-slow / mute transitions | n/a | Always **dynamic** (G1) |
| Region shift, teleport, render-type toggles, hide-selected, shader reload, atlas realloc, sink overflow | Global | Invalidate everything |
| Pass settings (avatar shadow detail, alpha-blend, LOD factor) | Parameter key | Value compare |

#### 2.11.2 Independent shadow subscriber (C3)

- `ALProbeDirty` gains a second subscriber flag `gShadowSubscribed`, set only by
  `WorldShadow` and never by `setRecording`.
- The hooks fire when `probe || shadow`. Events fan out **before**
  `classify`/provenance/tag filtering: the probe branch applies its filters,
  and the shadow branch has its own classification (the matrix).
- Each branch has its own fixed ring, overflow flag and drain. A shadow drain
  takes no time input. `setRecording(false)` clears only the probe side.
- **Gates:** shadows ON / probes OFF; probe reset with shadows ON; probe toggled
  every frame for 100 frames while a prim moves. The shadow ring receives every
  event exactly once.

#### 2.11.3 Cold and warm caches give the same picture (C4)

**Chosen: worst-case reservation.** Admission (§2.10.3) already assumes every
resident face re-renders every frame, which is P1-lite's uncached cost. A cache
hit only skips work, so cold, warm, overflow and re-entry frames show identical
visibility.

Gate: for 8 event sequences, final `areaVisibility` bytes with the cache equal
those with `RenderWorldShadowCacheVerify` (bypass).

A cache-readiness contract (warm-up frames before a take) was considered and
rejected: it makes output depend on history.

#### 2.11.4 Static/dynamic split

As Rev 1: copy the static tile to a composite, then draw the dynamic layer. It
is byte-equal to a single pass because depth with LESS ignores order. The camera
byte gate runs with frozen LOD only (C14).

#### 2.11.5 Variable tiers and packing (G2, C1)

- Only if P5 adds per-light tiers above the fixed grid. Gutters scale with the
  tier (`g = N/32`), so every tile is `N·17/16`: a power-of-two multiple of the
  base unit `B = 256·17/16 = 272`.
- **Buddy lemma:** place power-of-two squares in descending size into a
  power-of-two square quadtree. After all squares of size ≥ s are placed, the
  free space is a union of aligned s-blocks. So packing succeeds iff the total
  area ≤ the atlas area.
- The proof is exercised by an exhaustive pure test over all multisets of up to
  64 tiles in 4 tiers.
- Byte math: an atlas of 16 × 16 base units (4352² × 4 B) = **72.25 MiB**. It
  holds 64 tiles at 512 (10.7 point lights) or 16 at 1024 (2.7 point lights).
  The composite atlas is sized by the dynamic-face count actually admitted, with
  the same unit math.

### 2.12 Views

- **VCam / Prism feeds:** the atlas is world-space and shared. Feeds pay only
  their resolve inside their allowance. They never render or evict. Since all
  six faces always exist, feed cameras need no face-need registration (simpler
  than Rev 1).
- **Snapshots:** world visibility doesn't depend on resolution. Tiled
  snapshots are world-only, with contact off in tiles (U9).
- **Probes, mirrors, HUD, impostors, water:** excluded through
  `areaShadowsAllowedThisView()`.

### 2.13 Toggle, OFF identity, determinism

- `RenderWorldShadows` (default off, inside Soft shadows). Runtime uniforms and
  sampler bindings only: no define, reload or listener.
- **OFF:** no atlas, no subscriber, no extra uniforms, gather and upsample
  arithmetic identical to HEAD (independent-HEAD RGBA8 comparison, as Thin
  objects did).
- **Determinism:** no time, frame-count or cost input in residency, tiers or
  sampling. The residual inputs are those in §1.6 and the native fades (B9).
  Take records add mode, resident UUIDs, residency events, tier, and §1.6
  change counts.

---

## 3. Phased plan

Every phase follows the repo process: Codex implements, an adversarial Opus
review loops to zero must-fix, one build, the user tests in-world. Every phase
is default-OFF (except the approved P0 quality changes), runtime-toggled, and
OFF-identical.

### Phase 0: grain and acne fixes for the current screen-space shadows

**Sequenced after** the in-flight Thin objects planner change lands. Re-anchor
every line.

| Item | Change | Files |
|---|---|---|
| P0.1 Planner ladder and admission | See below. | `NV/alareashadowmath.h`, `NV/pipeline.cpp` (log, status), `alrelightmath_test.cpp` |
| P0.2 Low-discrepancy noise | Rotation and jitter from the **R2 rank** of the noise cell: `fract(u0 + 0.7548777·x + 0.5698403·y)`, `u0` = UUID hash (Cranley-Patterson shift). Same cell size (`area_params.z`). Blur radius ≥ 2 trace pixels (the Gaussian σ = r/√2 then spans more than the R2 correlation length). | `areaShadowF.glsl`, `NV/pipeline.cpp:27016` |
| P0.3 Filter edge-stops | The geometric normal from depth replaces the normal weight. A plane-distance weight `|dot(p'-p, n_geo)| / max(2 cm, 2·footprint)` replaces the raw dz. | `areaShadowFilterF.glsl`, `screenSpaceRayUtil.glsl` |
| P0.4 Horizon (**conditional**, U2) | Candidates: (a) skip samples with `dot(n_geo, dir) ≤ 0` and renormalize; (b) HEAD. Adopt (a) only if it lowers terminator-band **radiance** error against the reference below, with Light shape OFF and ON and on normal-mapped receivers. Otherwise drop it. | `areaShadowF.glsl` |
| P0.5 Geometric offset | Receiver offset and hit refinement use `n_geo`. Thickness `min(thickness, max(4·footprint, 5 cm))` for hits with `hit_t < 4·bias`. | `areaShadowF.glsl`, `screenSpaceRayUtil.glsl` |
| P0.6 Distance hole (**optional**, U13) | Per-receiver authority `α(z)` for mapped projectors with the full screen trace beyond the native cutoff (§2.9). Separable: ships only on yes. | `areaShadowF.glsl`, `NV/pipeline.cpp:27037-27046`, `relight_shader_check.py` |

**P0.1 in detail (C9, O7).** A pure, integer ladder with no timing input:

- **Separate quantities:**
  - `K`, the requested cap;
  - `A ≤ K`, admitted lights;
  - the per-draw channel width (`channels`, at most 4, `:147`);
  - the main reservation, now `A`-based so the feed allowance (`:192-194`)
    follows;
  - feed allowances.
- `A` is computed from `(preset, width, height, K, budget, Thin)` only, never
  the visible set, so it stays stable as lights enter and leave the view.
- **Uniform plans**, rungs in order (first that fits at `A = K` wins):
  1. Resolution: full res only if the full-res plan keeps ≥ 12 rays.
  2. Rays down to 8.
  3. Steps down to `max(8, S/2)`.
  4. Rays down to 4.
  5. Steps down to 8 (Low's authored count; HEAD already goes to 1).
  6. Then reduce `A` (lowest-ranked lights lose screen shadows).
- **Thin plans:** near steps stay 48. The far segment thins only to `minFar`
  (`:72-87`, `:143`), at rungs 3 and 5. Rays as above. Then `A`. Then HEAD's
  uniform fallback (`workloadOrUniform`, `:160-166`).
- The ray floor is `R_floor` (U1: 4 recommended). `R_floor = 1` restores HEAD's
  behaviour.
- **Published losses:** `[RelightQ] admit A/K rays R steps S half H`, the
  status line `Soft shadows: 2 of 8 lights at 4K (budget)`, and take-record
  fields.

Worked numbers (budget 1500 M, Thin planned 1453 M; half-res 4K = 2,073,600
px):

| Case | HEAD | Rev 2, `R_floor` = 4 | `R_floor` = 2 |
|---|---|---|---|
| 4K Cinematic uniform, K8 | full res, **1 ray**, 22 steps, 8 lights | half, 4 rays, 16 steps, **8 lights** (1.06 G) | — |
| 4K Thin, K8 (80 steps) | 1 ray, 8 lights | half, 4 rays, **A = 2** (664 M/light) | half, 2 rays, **A = 4** |
| 1440p Thin, K8 | 1-2 rays | half, 4 rays, **A = 4** | A = 8 |
| 1080p Thin, K8 | low rays | half, 4 rays, A = 8 (1.33 G) | — |

**User-visible effect:**
- Penumbrae lose their salt-and-pepper look at 4K, Cinematic and K=8.
- Normal-mapped walls and distant floors stop speckling.
- Under budget pressure, some lights lose soft shadows instead (shown in the
  status line).
- **Shapes can change** where:
  - (a) a light is no longer admitted;
  - (b) uniform steps thin to the floor of 8 (HEAD already thins further);
  - (c) P0.4 is adopted.
- With P0.6: mapped projectors keep shadows on far receivers.

**Risk:** medium. A planner semantics change, plus the reference harness.

**Effort:** 6-9 days (P0.1 2-3, P0.2 1, P0.3 1, P0.4 with references 1.5-2,
P0.5 0.5-1, gates). P0.6 adds 0.5-1.

**Gates** (new `relight_grain_check.py`, both GPU drivers):
- **Independent oracles (C14)** against the **unfiltered analytic** disk-light
  profile, not a reference through the same filter:
  - **edge location:** 50% crossing within 1 full-res pixel;
  - **contrast:** umbra ≤ analytic + 3/255, lit ≥ 252/255;
  - **penumbra width:** 10-90% within 20% of `1.374 × r_s(d_r - d_b)/d_b`.
- **Noise:** filtered RMS against a 64-ray/64-step reference
  - ≤ 6/255 at Medium 1080p K4;
  - ≤ 8/255 for every preset, 1080p and 4K, K4/K8, Thin OFF/ON, at the default
    budget;
  - HEAD must fail 4K Cinematic.
- **Spectrum (O6):** on a uniform penumbra patch, no FFT peak above 2/255
  amplitude at any frequency. The 4×4-interleave mutation must fail.
- **Speckle:** lit-region pixels below 250/255 = 0 on flat, grazing 85°,
  curved finite-disk 0.3 m (only if P0.4 is adopted), and normal-mapped
  receivers. HEAD fails the normal-mapped case.
- **Horizon references (C10):** a Python Monte Carlo integrator gives
  **radiance**, not visibility: true area-light integral, geometric occlusion,
  4096 samples per pixel.
  - Inputs: sphere and cylinder receivers, sources of 0.1, 0.3 and 1 m, Light
    shape OFF and ON, plus a normal map with ±25° disagreement.
  - Metric: terminator-band radiance RMS for HEAD versus P0.4(a). The decision
    is recorded in the doc.
- **Planner:** literal tables for every preset × 720p/1080p/1440p/4K × K1-8 ×
  Thin ON/OFF:
  - rays ≥ `R_floor` or a logged `A < K`;
  - `A` is independent of the visible-set size (permutation and subset test);
  - Thin near = 48 and far ≥ `minFar` in every row;
  - frame and draw ceilings hold;
  - the existing railing and join gates pass.
- **Moving camera (report + no regression):** same-world-point frame-to-frame
  variance over a 60-frame dolly is ≤ 1.1 × HEAD.
- **Determinism and OFF:** readbacks byte-identical; existing contact, fade,
  VCam and housing regressions hold; masters OFF stay RGBA8-identical.
- **Mutations that must fail:** samples-first degradation; G-buffer normal in
  the filter; per-cell white noise; a time seed; `A` derived from the visible
  set; Thin near steps thinned.
- **P0.6 gates:** a receiver sweep across `clip.z..clip.w+5 m`:
  - visibility continuous (≤ 2/255 per 1 cm);
  - shadows present beyond `clip.w`;
  - native bytes unchanged;
  - the sync check fails if either copy of the `α(z)` code is edited.
- **Cost:** Medium 1080p K4 GPU ≤ 1.15 × HEAD (Tracy + timestamp query).

### Phase P1-lite: uncached production cube-render spike

**Goal:** prove the real integration and measure its cost before anything else
is built on it (C12, O4, O12).

- **User-visible effect:** with World shadows on, the top one or two point
  lights cast shadows from blockers that are off-screen, behind the camera or
  hidden. They don't swim, railings near lamps show up, and the edges are
  fairly hard (like classic SL). Debug selector:
  `off / world+contact / world only`. Status line as in §2.10.3.
- **Scope:**
  - `NV/alworldshadowmath.h` (pure: face matrices with widened FOV, grid slots,
    `B_max`, ledger);
  - `NV/alworldshadow.{h,cpp}` (atlas owner, residency Auto/Pinned, cull option
    A or B, `bucketShadowCullFace`, tile render, state self-check, counters,
    Tracy zones `WorldShadow::render/cull/face/resolve`);
  - camera IDs;
  - `pipeline.cpp` hooks: frame start before Prism, tile-width
    `DEFERRED_SHADOW_TARGET_WIDTH`, `RenderSpotLight` exclusion, the full-res
    world resolve in the upsample pass;
  - `GLSL/class1/deferred/worldShadowUtil.glsl`;
  - reserved uniforms (`indra/llrender/llshadermgr.h/.cpp`);
  - settings: `RenderWorldShadows`, `RenderWorldShadowLights` (1-2),
    `RenderWorldShadowResidency` (Auto/Pinned), debug selector;
  - one Director row + status readout.
- **Deliberately out:** caching, sink, PCSS, projectors, variable tiers,
  packing, feeds-specific work. Feeds just share the atlas.
- **Risk:** medium-high. This is the spike, which is the point.
- **Effort:** 9-13 days:
  - atlas, grid and widened faces 1.5;
  - cull options, IDs and face filter 2;
  - `renderShadow` quirks and state restore 1.5-2;
  - PCF, biases, full-res resolve and acne tuning 2-3;
  - minimum contact 0.5;
  - counters, status and settings 1;
  - gates and the native-equivalence fixture 1.5-2;
  - measurement session 0.5-1.

**Correctness gates:**
- **Pure:**
  - face matrices round-trip against the probe look/up vectors;
  - the widened FOV maps the usable interior to exactly 90° (corner texel
    centres within 1e-5);
  - grid slots are pure under all 720 permutations of candidate order;
  - `B_max(512, r)` literals.
- **Production casters** (in viewer, not analytic boxes): a fixture parcel with
  one opaque prim, an alpha-mask fence, an alpha-blend hair-like prim, a rigged
  mesh avatar, an animesh, an attachment, grass and a GLTF mask.
  - Per-face draw counters by pass class match the expected set (an attachment
    draws only in faces its bounds touch).
  - The GL state self-check is clean after 1,000 frames.
- **Native equivalence (G4):** one face rendered with exactly a native 90°
  projector slot's matrices. Single-compare visibility against the native map
  agrees on ≥ 99.9% of receiver pixels (the D24/D32F difference is the
  tolerance).
- **Off-screen and hidden blockers:**
  - a blocker behind the camera: world ≤ 0.05 in the umbra, while screen-only
    reads 1;
  - a hidden blocker matches a ray-cast oracle within one `texel_world`.
- **Seams:** every edge and corner with unrelated neighbour tiles; no step above
  2/255.
- **Acne, peter-panning, leaks, thin rail:** as §2.7. A 4 cm rail 3 m from the
  light at 512: darkness ratio in [0.8, 1.2].
- **Minimum contact:** box resting on the floor: shadow start ≤ 1 full-res pixel
  from the contact line with world+contact. No double edge beyond `d_c`.
- **Camera motion** (geometry and LOD frozen, C14): eight poses with the atlas
  byte-identical; the edge position on the receiver varies ≤ 0.5
  `texel_world`; same-receiver-point agreement within 2/255 (world only).
- **Residency:** Pinned holds across a 360° pan and a dolly. Auto changes only
  with a status line and a log line (counter-checked).
- **OFF:** independent-HEAD RGBA8 equality; no atlas
  (`LLRenderTarget::sBytesAllocated` unchanged); zero extra GL calls; no
  reload.
- **Zero per-frame allocation** on steady frames.

**Exit gate: Tracy measurement in an avatar scene (decides D1).**

- **Fixtures**, on a fixed parcel with fixed camera paths. The counts are
  recorded with each run, so ms are comparable:

  | Fixture | Content inside the two lights' spheres |
  |---|---|
  | F0 | Static only: about 400 static draws, about 300 k tris |
  | F5 | F0 + 5 avatars (each about 60 rigged draws, about 80 k tris, 1 animesh) |
  | F10 | F0 + 10 avatars |
  | F20 | F0 + 20 avatars |

- **Anchor (O4):** the same fixtures with **two native projector slots** aimed
  at the avatars, World shadows off.
- **Measured** at 1080p and 4K: CPU (cull, stateSort, submission per face),
  GPU (raster per face, resolve), draws, triangles and skinned meshes per face,
  cull option A versus B, and VO-cache cull counts.
- **Proposed targets** (RTX 5090; the user can change them at D1):
  - F10 with two resident lights: world shadows ≤ **3.0 ms CPU** and
    ≤ **1.5 ms GPU** at 1080p;
  - ≤ 2.5 × the two-native-slot anchor;
  - F0: ≤ 1.0 ms CPU, ≤ 0.5 ms GPU.

### Decision point D1 (user, about 0.5 day)

Inputs: the P1-lite numbers, the in-world A/B, and VO-cache counts. Options:
- **Go:** P2 (PCSS), then P3. Optionally raise `RenderWorldShadowLights` to 3-4
  (grid sizing + gates, ≤ 1 day).
- **Go, but costly in crowds:** pick U14 (FPS drops, or a "World shadows: 1
  light" preset), and/or schedule P5 caching (it only helps static content;
  avatars always re-render).
- **Stop:** keep P0 only. P1-lite stays an opt-in debug mode or is removed.

### Phase P2: PCSS with corrected units

- **User-visible effect:** world shadows soften by the light's real size, up to
  the map-side clamp. They are sharp at contact and widen with distance. With
  U16 "hand-off", big fixtures get wide penumbrae too.
- **Scope:** min-depth pyramid; non-compare sampler with hygiene; search,
  penumbra and anisotropic PCF (§2.6.2-§2.6.3); the clamp and hand-off; the
  rotation candidates; the world-tap ledger.
- **Risk:** medium (C11).
- **Effort:** 7-10 days.

**Gates:**
- **Width:** half-plane blocker at 0.05, 0.5 and 2 m; sources r = 0.05, 0.15,
  0.5 and 1 m (real rig sizes).
  - **Inside the caps:** 10-90% width = 1.374 × `r(z_r - z_b)/z_b` projected,
    within 15% + 1 `texel_world`. Monotonic in separation and in r.
  - **Above the caps:** clamp-only gives exactly the capped width; hand-off
    within 20% of the true 1.374 × width.
- **Units:** near-plane invariance (n = 0.03 vs 0.10, ≤ 1/255). Off-axis
  receiver at a face-corner direction: width within 20%. Projector FOV/aspect
  conversion (P4 reuses this).
- **Noise (G3):** for each rotation candidate at maximum `R_pcf` and 16/32
  taps, RMS against a 1024-tap reference ≤ 4/255. If no candidate passes, taps
  go to 32 or `R_pcf` drops. The decision is recorded.
- **Moving camera (C11):** a 60-frame pan and dolly with frozen geometry and
  LOD:
  - per tracked world point, std ≤ 2/255 and max frame-to-frame step ≤ 4/255;
  - no spectral peak above 2/255;
  - a 2 cm rod is detected in ≥ 99% of frames at maximum softness.
- **Seams** with the largest kernel: ≤ 2/255.
- **Sampler hygiene:** `GL_SAMPLER_BINDING == 0` on all units after the passes.
- **Mutations that must fail:** constant kernel; Rev 1's `w·n/d_r` radius;
  missing 1.374 factor in the oracle (the gate must still pass with the correct
  factor and fail with a wrong one); time rotation; missing per-tap plane;
  search ignoring `z_s`.
- **Cost:** world-tap ledger within ceiling; Cinematic 1080p resolve ≤ 0.6 ms
  GPU for 2 lights (benchmark target, F0).

### Phase P3: contact hand-off from the bias geometry

- **User-visible effect:** no double edges, no gaps where feet touch floors,
  crisp contact, cheaper than full screen rays.
- **Scope:** the per-pixel `d_c` (§2.8), tightened thickness, the false-contact
  rule, Thin skipped for world-owned lights, the `user_extra` semantics.
- **Risk:** medium.
- **Effort:** 4-6 days.

**Gates** at full and half trace resolution (G9), tiers 512 and 1024:
- Blockers at 0.02, 0.05, 0.15, 0.5 and 2 m:
  - the edge profile is monotonic;
  - beyond `d_c` byte-equal to world-only;
  - inside it, the 50% crossing is within 1 full-res pixel of the oracle.
- **Grazing:** 80, 85 and 89° light. Shallow blockers (1 cm high).
  Displaced-edge case. A large penumbra (r = 1 m) crossing `d_c`.
- **Coverage:** zero pixels with `d_req > d_hard` in range at default biases.
- **False contact:** a camera-side pole darkens by ≤ 2/255.
- **Peter-panning:** ≤ 1 full-res pixel. Thin rail passes with Thin OFF.
- **Mutations that must fail:** fixed 0.15 m cap (the 89° grazing gate fails);
  ignoring `b_s` (overlap gap); product composition; HEAD thickness on contact
  (false-contact gate).
- **Cost:** contact iterations per world light ≤ 25% of the HEAD Medium march.

### Phase P4: projector world tiles and native authority

- **User-visible effect:** projectors that lost the auction, or are above the
  slot cap, get world shadows. Mapped projectors keep the SL look up close and
  get world shadows beyond the native cutoff (the env-light distance hole is
  closed with maps, not screen rays).
- **Scope:**
  - the shared optical-origin helper (native matrices byte-identical);
  - projector tiles in the grid at a fixed tier;
  - FOV/aspect units;
  - `α(z)` with `full = min(world, contact)`;
  - residency extended to projectors (same Auto/Pinned rules).
- **Risk:** medium.
- **Effort:** 5-7 days.

**Gates:**
- Native refactor: identical matrices and native map bytes for 100 random
  cases.
- Projector tile against the native map: edge separation ≤ 1 full-res pixel and
  a single monotonic boundary.
- The 1,001-step fade sweep with a resident tile: ≤ 1/255 per 0.001.
- Cutoff sweep as in P0.6, now with world shadows: continuous, with shadows
  beyond `clip.w`.
- §10 counters: auction order, slot counts and count-before-filter unchanged.

### Phase P5 (optional): caching

- **Only if D1 or later measurements show static content dominates cost.**
- **Scope:** §2.11 in full (the matrix hooks, the independent subscriber,
  worst-case reservation, the static/dynamic split, optional variable tiers
  with the buddy lemma, U15 policy).
- **Risk:** high (ghost shadows).
- **Effort:** 12-18 days.
- **Gates:**
  - a delivered-hook test per matrix row;
  - the subscriber lifecycle gates (§2.11.2);
  - cold/warm/overflow/re-entry final-visibility equality (§2.11.3);
  - static/dynamic equivalence over 50 random avatar placements;
  - the exhaustive packing test;
  - memory: zero growth over 1,000 frames of churn;
  - mutations: drop each hook class, the probe filter applied to shadows, a
    warm-only admission.

### Phase P6: VCam feeds, snapshots, UI, takes

As in Rev 1 P5, minus face-need registration: feed resolve inside allowances;
tiled-snapshot world-only (U9); take-record fields (resident UUIDs, residency
events, §1.6 counts); scene allowlist; kit rows, keywords and help; generators,
row pin and the XUI gate.

**Effort:** 6-9 days.

**Gates:** main visibility byte-unchanged with 1, 2 and 4 feeds; feeds never
render (counter); 2×2 tiled snapshot within 2/255 of untiled; take round-trip;
XUI zero findings; `prebuild_guard -ChecksOnly`.

### Later (not costed)

- **P7:** alpha/forward receivers sample the atlas, giving hair local shadows
  (8-12 days).
- **P8:** probes and mirrors sample the atlas (5-8).
- **P9:** layered single-pass cube rendering (5-8).

### Effort summary

| Scope | Phases | Engineer-days |
|---|---|---|
| **First deliverable** | P0 + P1-lite | **15-22** (16-23 with P0.6) |
| Core | + D1 + P2 + P3 | **27-39** |
| Full without caching | + P4 + P6 | **38-55** |
| Full with caching | + P5 | **50-73** |

These are honest ranges for this codebase (G5). The P1-lite range is the least
certain, which is exactly why it is a spike with an exit gate.

---

## 4. User decisions

**Needed before P0 / P1-lite:**

| # | Question | Recommendation |
|---|---|---|
| U1 | Under budget pressure: fewer rays (grain) or fewer shadowed lights? See the P0.1 table: at 4K with Thin, a floor of 4 shadows 2 of 8 lights, a floor of 2 shadows 4, and HEAD shadows all 8 at 1 ray. | `R_floor = 4`, with a status line. |
| U3 | Mapped projectors: give the native map authority only where it actually shadows (§2.9)? | Yes. U3-B is withdrawn. |
| U13 | Ship P0.6 (screen trace beyond the native cutoff) with P0, before world maps? | Yes. It is about 1 day and fixes the 10/21 env-light hole now (verify the hole in-world first). |
| U8 | World-shadow residency: Auto (follows rank, with a status line) or Pinned (held until Re-pick)? | Pinned while shooting; Auto otherwise. Default Auto. |
| U14 | A crowded scene with resident lights: keep shadows and let FPS drop, or offer a "1 light" preset? There is never a per-frame demotion. | Keep shadows; status line shows the cost; add the preset after D1 if needed. |
| U15 | VO-cache culling may delete casters behind the camera. Exempt objects inside resident-light spheres? (touches region code, not §10) | Measure in P1-lite first; decide at D1. |
| — | P0 changes the ON look of the shipped Soft shadows with no "Classic" switch. OK? | Yes, as a quality fix. |

**Can wait until D1 or later:**

| # | Question | Recommendation |
|---|---|---|
| U2 | P0.4 horizon change | Decided by the radiance references, not taste. |
| U4 | Housing exclusion: emitting prim only, or its whole linkset? | Emitting prim (near plane 0.05 m, so the shade casts). |
| U5 | Front faces (like SL) or two-sided casters? | Front faces. |
| U6 | Alpha-blend casters (hair) cast world shadows like native? | Yes. |
| U7 | Memory default | P1-lite 13.55 MiB fixed; larger only after D1. |
| U9 | Tiled snapshots world-only? | Yes. |
| U10 | Save the master with scenes? | Yes. |
| U11 | When does world+contact become the default? | After the P3 in-world sign-off. |
| U12 | Caster LOD: camera LOD or forced high within light range? | Camera LOD in v1. |
| U16 | Big fixtures (> 0.25 m radius): clamp-only (sharper than true) or the screen-space wide-penumbra hand-off? | Hand-off, if the P2 width gate passes; otherwise clamp-only. |
| U17 | Build optional P5 caching? | Only if D1 shows static content dominates. |

---

## 5. Risks

| Risk | Mitigation |
|---|---|
| **Avatar-scene CPU cost** (six faces × rigged draws; SL is draw-call bound) | The per-face filter; the native-slot anchor; the P1-lite exit gate before more investment; U14. Caching never helps avatars. |
| **Camera-dependent casters** (§1.6) | Status-line counts; U12, U15; frozen-LOD gates; documented as inherent. |
| **Residency pops** | Fixed tiers, all faces, no cost demotion, Pinned mode, status line on every change. |
| **Wide penumbrae** (rig fixtures 0.3-2 m) | Hierarchical search, clamp and hand-off (U16); gates at real sizes. |
| **Contact blind band** | Per-pixel derived `d_c`; `B ≤ d_hard` built into tuning; zero-uncovered gate. |
| **Stale cache** (P5 only) | Coverage matrix with delivered-hook tests; independent subscriber; worst-case reservation. |
| **Light leaks / peter-panning** | Normal-offset cap; leak and contact fixtures; U5. |
| **GL state leaks** (sampler objects, viewport, scissor, polygon offset) | Self-check and dispatch-fixture assertions. |
| **Native fades vary with time** (B9) | Unchanged; documented. |
| **Coordination** with the uncommitted planner change | P0 starts only after it lands; re-anchor every line. |

---

## 6. Notes for whoever implements this

- Do not change `BDMergeMaxSpotShadows`, slot allocation, the auction, fades or
  count-before-filter (§10). Per-receiver authority changes only the gather's
  mix weight.
- Do not edit `shadowUtil.glsl`'s `sampleSpotShadow` or `spotLightF`. The
  gather's `α(z)` copy is sync-checked.
- Do not modify `bucketShadowCull`. Add `bucketShadowCullFace`.
- Keep off-screen = lit for the contact trace. The world map is the only source
  of off-screen occlusion.
- Confirm the `shadow_clip.w` hole in-world before calling it the cause of the
  10/21 env-light report.

## User decisions (10/22)

- U1: ray floor of 4. Fewer lights, smooth: the remaining lights show no soft shadow, and the status line reports the coverage.
- U8/U14: Pinned residency. Chosen lights keep world shadows for the whole shot; in heavy scenes the FPS may drop rather than the shadows changing mid-shot.
- P0 may change the shipped default look (less grain). Thin objects behaviour and OFF identity stay as specified.
- P0.6 (env-light distance hole): implemented in the Relight ranking + stack-fixes job (10/22), not in P0.
- Still open: U3/U13 native-slot authority, U15 VO-cache exemption, and the remaining U items. Decide them at the D1 decision point after P1-lite.
