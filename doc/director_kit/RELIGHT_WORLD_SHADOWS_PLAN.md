# Relight world-space soft shadows and acne cleanup: plan

2026-10-22. **Planning only.** Nothing was implemented, built, committed or
stashed. Source was read at HEAD `a483b9392f8` with `git show`, because another
agent is editing `pipeline.cpp`, `alareashadowmath.h` and the area-shadow
shaders and tests in the working tree (Thin objects review round 7, a planner
change). Every `file:line` below is a HEAD line. Re-anchor against that round
before implementing anything.

`NV/` = `indra/newview/`. `GLSL/` = `indra/newview/app_settings/shaders/`.
`.tmp/ref_private/` and all ReShade/iMMERSE files were not opened. The only
algorithm sources are public: PCF (Reeves 1987), PCSS (Fernando 2005),
normal-offset shadows (Holbert 2011), slope-scaled depth bias (`glPolygonOffset`),
receiver-plane depth bias (Isidoro 2006), interleaved sampling (Keller and
Heidrich 2001), shadow atlases, cube and dual-paraboloid maps, and static/dynamic
cached shadow maps. The repo's own sun ShadowDist P2 path already implements
two of these and is reused.

Statements marked **(inference)** or **(estimate)** need a fixture or an in-world
measurement before anyone relies on them.

---

## 0. Summary

**What the user gets:** shadows from local lights that come from world-space
shadow maps instead of the screen. They see blockers that are off-screen, behind
the camera or hidden, they stay put while the camera moves, they catch thin
railings within a few metres of the light, and they look like SL projector and
sun shadows. The screen-space Relight trace stays, but only for short contact
detail (the "SL map + contact" rule from step 2, applied to every light). The
grain in today's soft shadows is fixed first, in Phase 0.

**Top acne/grain causes (section 1.4):**
1. The work planner removes rays first: 1 ray per pixel at 4K Cinematic, and
   1-2 at 4K with Thin objects.
2. The bilateral filter is too small and stops working on normal-mapped and
   grazing receivers.
3. On curved receivers (avatars), disk rays that start below the surface
   horizon are counted as blocked.
4. Each pixel gets an independent white-noise seed.
5. The receiver offset and hit refinement use the normal-mapped G-buffer normal.

**Phases and effort (engineer-days):**

| Phase | What it delivers | Effort |
|---|---|---|
| P0 | Grain quick wins | 4-6 |
| P1 | Cached cube maps for points + PCF | 9-13 |
| P2 | PCSS | 4-6 |
| P3 | Atlas, projectors, static/dynamic caching | 11-15 |
| P4 | Contact hand-off | 4-6 |
| P5 | VCam, snapshots, UI, takes | 6-9 |

- **Core win** (P0 + P1 + P2 + P4) is **21-31** days.
- **Full scope** is **38-55** days.

**Decisions needed:** section 4, U1-U12. The most important are:
- U1: under budget pressure, accept grain or shadow fewer lights.
- U3: for a projector that already holds a native SL shadow slot, reuse that map
  or render our own.
- U5: front-face casters like SL, or two-sided casters.
- U7: memory default.
- U11: when "World + contact" becomes the default mode.

---

## 1. Dive

### 1.1 Native projector shadow maps today

| Aspect | What HEAD does | Anchor |
|---|---|---|
| Allocation | Depth-only targets, `allocate(w,h,0,true)`, 24-bit depth by default. Size is screen width × `RenderShadowResolutionScale`, or `BDMergeProjectorShadowResolution` (256-8192). The slot count is `BDMergeMaxSpotShadows`; the hardware ceiling is 6/8/10 by texture units. | `NV/pipeline.cpp:2731-2762`; `NV/pipeline.h:1519-1545`; `indra/llrender/llrendertarget.cpp:140-145` |
| Compare state | `GL_COMPARE_REF_TO_TEXTURE`, `GL_LEQUAL`, anisotropic filter, clamp. | `NV/pipeline.cpp:2794-2808` |
| Slot auction (off-limits, §10) | `setupSpotLight` bubbles priority: rig tier, then world, then actor-only. A Cast-shadows opt-out skips it. Fades use `gFrameIntervalSeconds` × camera velocity, so they depend on time. | `NV/pipeline.cpp:29248-29310`, `:31872`, `:31966-31975` |
| Map generation | Every frame, for every held slot: rebuild the optical origin from prim scale/FOV, near = apex + 0.01, far = apex + 1.5 × radius, then `updateCull` + `stateSort` + draw. **No caching.** The light's own prim is excluded (`RenderSpotLight`). | `NV/pipeline.cpp:32020-32136` (origin `:32070`, far `:32085`, render `:32130`); `NV/pipeline.cpp:5717` |
| Shadow pass content | Static and rigged passes (`include_rigged=true`), the geometry pools (avatars), alpha-mask and fullbright-mask, alpha blend (`include_alpha_blend=true`), grass, GLTF mask, back-face culling on. Occlusion culling is off. | `NV/pipeline.cpp:30040-30330`; `NV/pipeline.h:816-822` |
| Avatars | No shadows from impostored, jelly-dolled, invisible, muted or "too slow" avatars. `RenderAvatarShadowDetail` can drop the alpha-blend and alpha-mask passes. | `NV/lldrawpoolavatar.cpp:480-547` |
| Bias | Receiver offset is a constant `spot_shadow_offset` (0.04 m) along the G-buffer normal. Depth bias is `0.8 × RenderSpotShadowBias` (-0.0002) in **post-projective depth units**, so the bias in metres grows roughly with distance². | `GLSL/class1/deferred/shadowUtil.glsl:1023`, `:454`; `settings.xml:20482-20503` |
| Filtering | Classic: a 5-tap snapped kernel. Optional "PCSS-lite": a 4 × 6 compare-sampler blocker-gap estimate, because a comparison sampler can't return raw depth. The kernel grows with gap × softness, under a hard cap. There are 12-tap Poisson or Vogel taps with a spatial-only rotation, and a fill floor. | `shadowUtil.glsl:160-184`, `:450-535` |
| Sampling range | A projector map is only sampled where the receiver is inside the **sun shadow far clip**: `spos.z > -shadow_clip.w`, otherwise lit. | `shadowUtil.glsl:1026`, `:1092`; `NV/pipeline.cpp:25142` |
| Sun world-units path (reusable) | The receiver offset along N is texel-scaled × sin(angle). There is an exact map-space receiver-plane Jacobian with a per-tap plane-following depth, a slope cap, and a millimetre floor. It works for perspective and orthographic maps. | `shadowUtil.glsl:368-440`, `:272-358` |

**Which costs dominate:**
- **CPU:** one octree cull + `stateSort` + full draw submission per map per
  frame, with nothing cached. Rigged avatars are drawn in a second pass.
- **GPU:** depth raster of everything in the frustum, including re-skinning
  rigged avatars per map, plus the alpha-mask texture fetches.
- **Prism feeds** repeat the whole thing into `mPrismSpotShadow`
  (`NV/pipeline.cpp:32199+`).
- Sun cascades already have a one-cull-shared-by-many-frusta option
  (`RenderShadowCullMode 1`, `bucketShadowCull`, `NV/pipeline.cpp:30640-30676`,
  `:31074`, `:31820`). Projector maps don't use it.

**Finding outside this task (inference, verify in-world):** because of the
`shadow_clip.w` test, a projector that **holds** a native slot casts no map
shadow on receivers farther from the camera than the sun's far shadow split. The
step-2 contact-only trace (fade 1) also drops distant screen shadows. So a
settled, mapped projector can show **no shadow at all** on far receivers. This
fits ranked hypothesis 3 of the 10/21 "Env lights regression" note better than
the hypotheses tested there. World tiles (P3) remove this limit for any light
they own.

### 1.2 Can the same machinery render cube maps or an atlas?

**Yes, with small, local changes.**

- `renderShadow(view, proj, LLCamera&, LLCullResult&, ...)` is generic over the
  view and projection matrices (`NV/pipeline.cpp:30040`). A cube face is a
  90-degree, aspect-1 perspective camera. The probe code already defines the six
  look/up vectors (`NV/llviewerwindow.cpp:6500-6517`) and sets up a 90-degree
  camera (`:6444-6447`).
- **Cull once per light.** Cull the light's sphere once, then reuse
  `bucketShadowCull` per face. Bridges (attachments, animesh, moving linksets)
  pass through unfiltered (`NV/pipeline.cpp:30665-30675`). That is exactly the
  dynamic layer we need.
- **Atlas target.** One `LLRenderTarget` with `DEPTH_FMT_32F`
  (`llrendertarget.cpp:140`, `llrendertarget.h:75-76`).

Gaps to close:
1. `bindTarget` sets a full-target viewport. Tile rendering needs an explicit
   viewport + scissor and a scissored depth clear.
2. `renderShadow` sends `LLRenderTarget::sCurResX` as
   `DEFERRED_SHADOW_TARGET_WIDTH` (`:30219`, `:30233`). This must be the **tile**
   width, or alpha-mask behaviour changes.
3. Per-camera spatial-group state needs a camera id. Add one
   `CAMERA_LOCAL_SHADOW` (`NV/llviewercamera.h:47-69`), not one per face.
4. `LLCullResult` storage must be fixed-size per face/tile slot. The
   `result[MAX_SPOT_SHADOWS]` crash comment at `NV/pipeline.cpp:32124` is the
   precedent.
5. Raw depth reads for the PCSS blocker search. Bind the same texture through two
   **sampler objects**, compare and non-compare (`glGenSamplers` /
   `glBindSampler`, loaded at `indra/llrender/llgl.cpp:619-622`). Alternatively,
   use `textureGather` with manual compares.

**Cube layout.** Six 90-degree faces per point, unwrapped into 2D atlas tiles.

| Option | Verdict |
|---|---|
| **Six faces as atlas tiles** | Chosen. Straight edges, uniform texels, reuses `renderShadow` unchanged. |
| Dual-paraboloid (2 renders) | Rejected: needs dense tessellation. Large flat SL prims and boxes rasterize wrongly. |
| Tetrahedral (4 faces) | Rejected for v1: about 110-degree faces and awkward tiles. |
| Single-pass layered (`gl_Layer`) | Later: every shadow program would need new permutations. |

Most of the six-render cost is removed by face culling against the active view
frusta (§2.3) and by caching (§2.4).

### 1.3 How reflection probes render cube faces, and the cost

- `LLViewerWindow::cubeSnapshot` renders **one face per call**
  (`NV/llviewerwindow.cpp:6420-6530`) through `display_cube_face()`
  (`NV/llviewerdisplay.cpp:1426`). That is a full `updateCull` (`:1498`),
  optional sun-shadow generation (`:1515-1520`), `stateSort` (`:1531`), and a
  complete deferred G-buffer + lighting pass.
- The manager spreads faces over frames (`NV/llreflectionmapmanager.cpp:963`,
  `:1039`, `:2378`).
- So the probe path gives us **orientation conventions and amortization
  precedent only**. Its per-face cost (full deferred render) is the wrong model.
  Shadow faces must be depth-only `renderShadow` calls.
- The probe on-demand system has the event recorder we need for caching:
  `ALProbeDirty` (`NV/alprobedirty.h:39-206`). It has drawable xform deltas
  (`:157`), insertion and removal (`:162`), light-volume changes (`:165`),
  texture arrival and downscale (`:169-170`), LOD tags (`:65`) and region shift.
- But its drain **debounces on wall-clock time** (`drain(Drain&, F64 now)`
  `:204`; `alprobedirty.cpp:324-328`, `:666-674`), and it drops LOD-driven
  rebuilds as noise. Shadows need a **timing-free raw sink** fed from the same
  hooks (§2.4).

### 1.4 Where today's soft-shadow grain and acne come from (ranked)

| Rank | Cause | Evidence | Where it shows |
|---|---|---|---|
| 1 | **Ray starvation by the work planner.** `workloadWithin` drops samples to 1 **before** it reduces steps or resolution. Cinematic asks for full resolution with 24 rays, which costs 4× and forces the ray count down. | `NV/alareashadowmath.h:137-140`, presets `:70`. HEAD tables (`RELIGHT_SL_INTEGRATION.md:1627-1665`): Cinematic 1080p K4 = 5 rays, K8 = 2; 4K Cinematic = **1 ray** (K4 and K8); 4K Medium K8 = 7; Thin objects ON at 4K K8 = **1 ray for every preset**. The Thin-objects help text and round-5 decision say so too. A binary estimate with S rays has standard deviation `sqrt(p(1-p)/S)`, which is 0.5 at S=1 and 0.18 at S=8 at mid-penumbra. | Every penumbra at 4K and in Cinematic; it worsens with K=8 and Thin objects. Cinematic can look grainier than High. |
| 2 | **The filter is small and its edge-stops collapse.** The blur radius is `denoise × H/1080 × (half ? 0.5 : 1)`, so **1 trace pixel** (3 taps) at the 1080p half-res default. The normal weight is `pow(dot(n,n'),32)` on **normal-mapped** G-buffer normals: a 15-degree bump difference gives 0.33, 25 degrees gives 0.04. The depth weight `exp(-dz/max(0.03, 0.005z))` uses raw depth difference. On a floor 20 m away seen from 1.7 m, adjacent pixels differ by about 0.23 m against sigma 0.1 m, weight about 0.1 **(estimate)**. | `NV/pipeline.cpp:27014-27016`; `GLSL/class3/deferred/areaShadowFilterF.glsl:12-16`, `:33-39` | Speckle that survives filtering on brick, stone and fabric normal maps, and on distant floors and grazing walls. |
| 3 | **Self-shadowing at the horizon on curved receivers.** Disk samples below the receiver's tangent plane march into the surface. The front depth is then in front of the ray, so it becomes a candidate, refines to `hit_t ≈ 0+`, validates, and counts as blocked. Light shape's sphere shading already integrates the horizon, so this darkens twice and adds noise along the terminator. | Sampling `GLSL/class3/deferred/areaShadowF.glsl:54-75`; crossing/refinement `GLSL/class1/deferred/screenSpaceRayUtil.glsl:107-129`. The HEAD curved-receiver fixture only passes the grazing band with a **point-like aperture** because "a finite disk can physically straddle the terminator" (`RELIGHT_SL_INTEGRATION.md:1698-1701`). | A grainy band along terminators on avatars and round props, wider with large source sizes (rig fixtures 0.3-2 m). |
| 4 | **White-noise seeds per pixel.** Each trace pixel hashes its own rotation and jitter, so neighbouring pixels share no structure, and white noise is the hardest kind for a 3-tap spatial filter to remove. | `areaShadowF.glsl:39`, `:47-49`, `:57-58` | Amplifies causes 1 and 2. |
| 5 | **Offset and refinement use the shaded normal.** The receiver origin is offset along the normal-mapped normal (`areaShadowF.glsl:37`, `screenSpaceRayUtil.glsl:30-34`). The footprint bias is `max(2 cm, 1.5 × footprint)` at receiver depth (`:38-45`). Refinement and validation intersect against the hit's **normal-mapped** plane, with 0.35 m thickness acceptance (`:112-125`). | Code. The B1 lens attempts were rejected partly for "receiver acne" (`RELIGHT_SL_INTEGRATION.md:1027-1028`). Magnitude is an **inference**; it needs the normal-mapped fixture in P0. | Isolated dark specks on strongly normal-mapped flat receivers and near creases. |
| 6 | **Half-res upsample at edges.** Four-tap bilinear with the same collapsing weights; if every weight is below 1e-8 it falls back to nearest. | `areaShadowFilterF.glsl:44-62` | Stair-step speckle along silhouettes, in half-res presets only. |
| 7 | **Ruled out: depth precision and RGBA8.** 24-bit depth error at 64 m with a 0.1 m near plane is about z²/(n·2²⁴) ≈ 2.4 mm, well under the 2 cm bias floor. RGBA8 continuity is measured at 1-3/255 (`RELIGHT_SL_INTEGRATION.md:1128`). | Arithmetic and existing gates | Not a visible cause. |

### 1.5 What to reuse

| Asset | Reuse for world shadows | Anchor |
|---|---|---|
| Selection and ranking | Same pure selector: unmapped first, contribution, UUID; K ≤ 8; suppression, mirrors, actor-only, collector intersection. World residency is granted in that rank order. | `NV/pipeline.cpp:26825-26865`; `NV/alareashadowmath.h:24-54` |
| Light size | `area_source_radius`: fixture `mSourceSizeM/2`, then Size from prim, then World light size, then the 0.10 m shadow-only fallback. Sets the PCSS light size. | `NV/pipeline.cpp:11784-11803`; `areaShadowF.glsl:51` |
| Budget planner | Extend the same 64-bit, pure, no-timing ledger with resolve-tap and raster terms. The K reservation and feed allowances stay. | `NV/alareashadowmath.h:102-198` |
| Visibility channels and consumers | **The key reuse.** World visibility is resolved into the existing RGBA8 `areaGather`/`areaVisibility` channels. The point, multi-point and spot consumers stay byte-unchanged (`min` for projectors, multiply for points). | `GLSL/class3/deferred/spotLightF.glsl:196-197`; `pointLightF.glsl:298-299`; `multiPointLightF.glsl:169-170`, `:226-227` |
| Step-2 contact classification | `areaTrace` already returns full and contact visibility from one march, and the gather blends by native fade. World-owned lights use "contact only" (fade 1 semantics) on day one of P1. | `screenSpaceRayUtil.glsl:46-133`; `areaShadowF.glsl:71-78`; `NV/pipeline.cpp:27030-27047` |
| Thin objects | Kept for **screen-only** lights. World tiles resolve a 4 cm rail out to about 5 m (512 faces) or 10 m (1024 faces) from the light (§2.3), so world-owned lights don't need the dense march. | `screenSpaceRayUtil.glsl:55-88` |
| Geometric normal from depth | Five taps, minimum difference per axis, falling back to the shading normal. Used for receiver offsets, filter edge-stops and horizon tests. | `GLSL/class1/deferred/rigRimAuxF.glsl:124-150` |
| Sun receiver-plane bias | Port `sunBandC`/`pcfShadowC` math to local perspective tiles. | `shadowUtil.glsl:272-440` |
| Native optical origin | Factor the projector apex/near/far construction into one shared helper, so world projector tiles use identical matrices. The refactor must leave the native matrices byte-identical. | `NV/pipeline.cpp:32050-32101` |
| View policy | `areaShadowsAllowedThisView()` and the `ALAreaShadow::View` flags stay the single gate. | `NV/pipeline.cpp:26612-26622`; `alareashadowmath.h:10-18` |
| Director UI | Real card `kc_area_soft_shadows`. New rows go there with search keywords. The generators and row pin are as today. | `NV/skins/default/xui/en/panel_dpage_light_rim.xml:234-295`; `scripts/code_tools/director_kit/director_mockup_data.json` |
| Scene save and takes | Shared scene allowlist and sparse load; take records include settings, selection, work and slots. | `NV/llfloaterdirector.cpp:3743-3744`; `NV/alrelighttakerecord.h:11-48`; `NV/pipeline.cpp:26658-26712` |
| VCam feeds | Scoped feed pack, save/restore transaction, feed allowances, `FeedPasses`. Feeds render **before** the main view (`NV/llviewerdisplay.cpp:944`). | `NV/pipeline.cpp:11222`, `:11521`, `:11686`; `alareashadowmath.h:168-198` |
| Event recorder | `ALProbeDirty` hooks, plus a new timing-free shadow sink. | `NV/alprobedirty.h:42-170` |
| Test harnesses | `relight_area_check.py`, `relight_shader_check.py`, `relight_thin_check.py` (independent-HEAD RGBA8 OFF identity), `relight_vcam_dispatch_check.py` (real GL ownership and allocation counters), `alrelightmath_test.cpp`. | `scripts/code_tools/` |

---

## 2. Target architecture

### 2.1 Frame flow

```
frame start (before LLPrismLens::renderAuxiliaryView, llviewerdisplay.cpp:944)
  WorldShadow::plan()       pure: selection per active view -> resident set,
                            tiers, canonical atlas packing, face-need set,
                            raster admission (no timing input)
  WorldShadow::invalidate() drain the timing-free shadow sink -> mark tiles
  WorldShadow::render()     static tiles that are invalid and needed;
                            dynamic composite tiles (copy static + draw movers)
per view (Prism feed / main), inside renderAreaShadows (pipeline.cpp:27055):
  gather (existing program, new runtime mode):
     world-owned light: vis = min(PCSS(atlas), contact_trace(short))
     screen-only light: today's trace (unchanged arithmetic)
  existing H/V bilateral + full-res upsample -> areaVisibility planes
  existing consumers (point x vis, projector min(native, vis)): UNCHANGED
```

Light shaders don't change. The atlas is sampled only by the gather program.

### 2.2 Which lights get maps

- **Point lights:** six 90-degree faces in the atlas, near plane = the housing
  bias (`RenderAreaShadowLightBias`, default 0.25 m), far plane = 1.5 × radius.
  The light's own prim is excluded through `RenderSpotLight`
  (`NV/pipeline.cpp:5717`). Whole-linkset exclusion is U4.
- **Projectors without a native slot:** one perspective tile built with the
  shared optical-origin helper (§1.5). This includes projectors that lost the
  auction, slots above `BDMergeMaxSpotShadows`, and Preferences shadow detail
  below 2.
- **Projectors with a native slot (default, U3 option A):** keep the native map
  and today's step-2 hybrid. The §10 auction is untouched, there's no double
  render, and the look is exactly SL.
  - While the slot fades, the gather blends from `min(world, contact)` (if the
    light has a resident tile) to contact-only, using the existing
    `area_contact_fade`.
  - Option B ("own map always") is deterministic and has no camera-distance
    cutoff (§1.1 finding), but costs one extra cached tile per mapped projector.
    It also needs `spotLightF` to skip the native sample for atlas-owned lights.
    That is a consumer change, not a §10 change.
- **Lights not selected, or without residency:** unchanged screen-only trace.
  World shadows are never all-or-nothing.

### 2.3 Resolution and deterministic allocation

All of these are pure functions of the current frame's inputs.

- **Texel size:** a face of size N at distance d has a world texel of 2d/N. A
  512 face gives 7.8 mm at 2 m and 3.9 cm at 10 m. A 1024 face gives 2 cm at
  10 m.
- **Tier:** `N = clamp(nextPow2(q · s), Nmin, Nmax[preset])`, where
  `s = range / max(dist - range, near) × (viewport_h / (2 tan(fov/2)))` is the
  on-screen size of the influence sphere and q = 1. Presets:
  - Medium: 256-512
  - High: 512-1024
  - Cinematic: 1024 for points, 2048 for projectors
- **Rank cap:** after the tiers are set, if the total area exceeds the atlas,
  halve the tier of the lowest-ranked light (ties by UUID) and repeat. No
  hysteresis.
- **Packing:** sort tiles by size (descending), then rank, then UUID, then face
  index, and pack them into a power-of-two quadtree. With power-of-two squares in
  that order, packing always succeeds when the total area fits, so the layout is
  canonical.
  - A tile that moves between frames is copied (`glCopyImageSubData`), not
    re-rendered.
  - Sampling is invariant under integer tile translation, so the results don't
    depend on placement.
- **Guard band:** 4 texels per face, to cover bilinear filtering and the
  blocker-search footprint.
  - PCSS taps pick their cube face **per tap** from the tap's world direction,
    so penumbrae cross face seams without artefacts.
- **Face need:** a face is needed when (face frustum ∩ light sphere) intersects
  any **active view frustum** this frame. Active views are the main camera plus
  the registered feed cameras, known at frame start. Faces that aren't needed are
  not rendered. Their cache entries still receive invalidations.
- **Capture pin (U8):** during a take, tiers can be frozen to the values at take
  start, so a dolly move never re-tiers a light.

### 2.4 Caching: "changed" defined without any timing input

Each tile stores its **parameter key**: the light's UUID, position, rotation,
radius, FOV and scale, source radius, tier, near plane, the cast-shadow flags,
and the settings that affect the shadow pass (avatar shadow detail, render-type
mask). It also stores a **valid** bit.

The static layer is **invalid** this frame if any of these holds:

1. The current parameter key differs from the stored one (a value compare).
2. The **shadow sink** holds a raw event this frame whose world AABB intersects
   the face frustum ∩ light sphere. Raw events are:
   - insertion or removal outside a move;
   - an xform delta of a static drawable;
   - a geometry or LOD rebuild (`REBUILD_VOLUME`/`REBUILD_POSITION`, LOD tags
     **not** dropped);
   - a texture arrival or downscale on alpha-mask faces (cut-outs change).
3. A global event: sink overflow, region shift or teleport, atlas reallocation,
   shader reload, or a change to a render-type or visibility toggle such as hide
   selected or mute. These invalidate everything.

The sink is a fixed-capacity ring fed by the existing `ALProbeDirty` hooks. It
is enabled when either probe recording or world shadows is on, has **no
debounce**, and is drained once per frame. Overflow invalidates everything,
deterministically.

**Dynamic layer.** These are the casters inside bridges or avatar partitions:
- avatars, attachments, animesh;
- active (moving) prims;
- the `bucketShadowCull` visible list and bridges.

For every needed face whose frustum contains a dynamic caster's bounds, every
frame:
1. Copy the static tile into a composite tile.
2. Draw the dynamic casters on top.

A static scene therefore costs only the resolve.

**Why this is deterministic.** Depth with a LESS test is independent of draw
order. So, given the current geometry, a correctly invalidated cached tile is
**byte-equal** to a fresh render. A cache-bypass mode
(`RenderWorldShadowCacheVerify`, debug) re-renders everything and compares.
Network-timed object updates are inherent inputs, as already documented under
B9.

### 2.5 What the shadow pass contains

- Same as native `renderShadow`: static and rigged passes, avatars through the
  pool shadow passes, animesh (`RENDER_TYPE_CONTROL_AV`), alpha-mask and GLTF
  mask, grass, and **alpha-blend** with native thresholds (U6, default on, so
  hair shadows match SL projectors).
- `RenderAvatarShadowDetail`, impostor/jelly-doll/mute/invisible exclusions and
  hidden entity clones follow `lldrawpoolavatar.cpp:480-547` unchanged.
- Particles: none, as native.
- Back-face culling ON, as native (U5).
- Slope-scaled raster bias: `glPolygonOffset` for atlas draws only (§2.7).

### 2.6 Filtering: PCSS (Fernando 2005)

- **Inputs:** linear receiver distance `d_r`, light radius `r_L` from
  `area_source_radius` (§1.5), and face near plane `n`.
- **Blocker search** (16 raw-depth texels, either 4 `textureGather` calls or
  through the non-compare sampler):
  - search radius in face UV = `(r_L / (2 n tan45)) · (d_r - n) / d_r`;
  - average blocker distance `d_b` over texels nearer than `d_r - bias`;
  - no blockers means lit (early out); all blocked across the full search means
    the umbra test.
- **Penumbra:** `w = r_L · (d_r - d_b) / d_b`, projected to filter radius
  `w · n / d_r`. It is clamped to a per-preset cap in texels. The cap is never
  hit at the default sizes, and that is gated.
- **PCF:** a Vogel disk of 16 taps (32 at Cinematic). Each tap is a hardware
  bilinear compare, so each tap returns a continuous value. Each tap applies the
  receiver-plane depth offset (§2.7).
- **Rotation:** the same integer hash of pixel cell + light UUID as today, laid
  out as a **4×4 interleaved** pattern. No frame, time or channel input.
- **Contact hardening:** `d_b → d_r` drives w → 0. A zero radius uses the
  existing 0.10 m shadow-only disk, so world shadows match the screen-space
  penumbra scale.

### 2.7 Preventing acne, and how to tune it

Per tap, in this order:

1. **Normal offset along the geometric normal** (depth-reconstructed, §1.5):
   `k_n · texel_world(d) · sin θ`, where θ is the angle between N_geo and L.
   It is capped at `min(2 texels, 5 cm)` to limit light leaks through thin
   walls.
2. **Receiver-plane depth bias** (Isidoro 2006): the exact perspective Jacobian
   from `sunBandC` (`shadowUtil.glsl:398-438`), using N_geo. Every PCF and search
   tap compares against the receiver plane at its own UV. The slope cap is 8,
   as in the sun path.
3. **Constant floor** `b_mm` in millimetres, converted to depth at the receiver.
4. **Slope-scaled raster bias** during atlas rendering:
   `glPolygonOffset(f_slope, u)`.

**Tuning procedure.** Run it per tier, and record the result in the harness as
literals.

1. Set every bias to 0 and confirm each acne fixture **fails** (this proves the
   fixtures can fail).
2. Raise `k_n` from 0 in steps of 0.25 until the flat-45-degree and curved
   fixtures pass the acne gate.
3. Raise `f_slope` from 0 in steps of 0.5 until the grazing (80/85/89 degree)
   fixtures pass.
4. Raise `b_mm` from 0 in steps of 0.5 mm until the normal-mapped fixture passes.
5. Measure peter-panning on the contact fixture. If the shadow start is more than
   max(1.5 texel_world, 2 cm) from the contact line, back off the largest
   contributor and repeat.
6. Run the leak fixture (5 cm wall). Any leak means lowering the `k_n` cap.

**Acne gate:**
- On an unoccluded receiver, at least 99.9% of pixels with N_geo·L > 0.05
  must read ≥ 254/255.
- No connected component of 3 or more pixels may read below 250/255.

### 2.8 Composition with the screen-space contact trace

- **World-owned light:** `vis = min(PCSS_world, contact)`. `min` and not a
  product is the existing convention (`spotLightF.glsl:196-197`), and the tested
  multiply mutation is rejected.
- **Contact trace for world-owned lights:** a **short dense march** over
  `[bias, d_c]` only. There's no fade for these lights, so the step-2 objection
  to length-capping doesn't apply.
  - 8 steps over 0.15 m gives 1.9 cm spacing, much denser and cheaper than the
    full-length 12-80 step march.
  - Near-contact penumbrae are small, so 4 contact rays suffice.
- **Hand-off distance:**
  `d_c = clamp(4 · texel_world(receiver, tile), 2 cm, RenderAreaShadowMappedContact)`.
  The contact band covers the map's bias blind zone (normal offset +
  receiver-plane band ≈ 1-3 texels) and nothing farther. Beyond `d_c` only the
  world map can shadow, so a far blocker can't produce a second, displaced
  screen edge.
- **Mapped projectors in a fade:** the existing `mix(full, contact, fade)`, with
  `full` replaced by `min(world, contact)` when the light has a resident tile.

### 2.9 Budget, memory and the VRAM governor

- **Resolve taps** (search + PCF + contact steps) per trace pixel per world light
  go into the **same** `RenderAreaShadowBudget` iteration ledger, one tap = one
  iteration, under the same K reservation and feed allowances.
- **Raster admission:**

  | Budget | Default | What it bounds |
  |---|---|---|
  | `RenderWorldShadowFaceBudget` | 12 | Static face re-renders per frame |
  | `RenderWorldShadowDynamicFaces` | 24 | Dynamic composites per frame |

  Over budget, degrade in a fixed order:
  1. Lower the tier of the lowest-ranked lights.
  2. Then demote whole lights to screen-only for that frame.

  Never sample a stale tile. Every degradation is logged as `[RelightQ]` with
  its inputs.
- **Memory (default):**

  | Target | Size | Memory |
  |---|---|---|
  | Static atlas, D32F | 4096² | 64 MiB |
  | Composite atlas | 4096 × 2048 | 32 MiB |
  | **Total** | | **96 MiB** |

  - 24 faces at 512 (four lights) use 24 MiB of the static atlas.
  - Cinematic 8192² (256 MiB + 128 MiB) is opt-in (U7).
- **Governor:** render-target bytes are already counted in texture accounting
  (`NV/llviewertexture.cpp:654-657`) and raise the fixed floor (`:676-677`).
  Admission picks the atlas size from **static** inputs: the configured or
  effective budget at enable time. It never reacts to live pressure, which would
  make output depend on history. If the reservation doesn't fit, it fails closed
  to a smaller atlas, with a status line.

### 2.10 Views

- **Main:** as above.
- **VCam/Prism feeds** (`RenderAreaShadowsVCam`): the atlas is world-space, so
  every feed **shares** it. A feed pays only its gather/resolve inside its
  existing allowance. The face-need set includes the feed cameras registered for
  the frame. A light a feed selects that has no residency uses the feed's
  screen-only trace. Feeds cannot render, evict or invalidate tiles; only
  `WorldShadow::render()` at frame start does.
- **Untiled and single-target high-res snapshots:** world visibility does not
  depend on resolution. PCSS is in world units, so the penumbra doesn't change
  with output size.
- **Tiled snapshots:** world-only is allowed in tiles (U9). The atlas is
  rendered once for the full snapshot frustum before the first tile. The contact
  trace stays off in tiles to avoid seams.
- **Probes, hero mirrors, HUD, impostors, water reflection:** stay excluded
  through `areaShadowsAllowedThisView()`.
- **Later:** probes could sample the atlas for consistent local-light shadows in
  captures. Forward/alpha receivers could look the atlas up per fragment, which
  finally gives hair local-light shadows. That would be D6 done properly; see
  P6.

### 2.11 Toggle, OFF identity, determinism

- **New persistent creative setting `RenderWorldShadows`** (default off, inside
  Soft shadows). It adds a runtime `area_world` uniform and sampler bindings to
  the existing gather program: no define, no reload, no listener.
- **When OFF:** no atlas allocation, no sink recording (unless probes record),
  no extra uniform uploads, and gather arithmetic identical to HEAD. This is
  proven the same way Thin objects did it: an independent-HEAD RGBA8 comparison
  plus the disabled-expression and no-reload checks.
- **Prepare:** allocate lazily on the first ON frame and release on OFF. If the
  measured first-enable hitch is more than one 60 Hz frame on the RTX 5090, add
  an AO-plan-style "Prepare world shadows" machine switch, so allocation never
  lands mid-take.
- **Determinism:** no time, frame-count, channel or history input in the
  sampling; tiers are pure; packing is canonical; caching is verified
  byte-equal. The residual limits are the existing native fade, collector and
  network-timing limits (B9). Take records add world mode, per-UUID tier and
  tile, cache hit or re-render per face, and degradation verdicts.

---

## 3. Phased plan

Every phase follows the repo process: Codex implements, an adversarial Opus
review loops to zero must-fix, Claude builds once, the user tests in-world.
Every phase is default-OFF, runtime-toggled and OFF-identical.

### Phase 0: grain and acne quick wins for the current screen-space shadows

**Ships first; independent of world maps.** It must be sequenced **after** the
in-flight Thin objects round 7 lands, because it touches the same planner.

| Item | Change | Files |
|---|---|---|
| P0.1 Ray floor | Planner order: if fewer than 12 full-res rays fit, choose half resolution (extend `kFullResMinSamples` to the uniform path). Then reduce steps to a floor of 8. Then rays down to a **floor of 4**. Then split draws. Then drop the lowest-ranked channels (U1), logged. Never a 1-ray plan unless U1 says "grain". | `NV/alareashadowmath.h`, `NV/pipeline.cpp` (log), tests |
| P0.2 Interleaved sampling | A 4×4 interleaved rotation/jitter index (pixel `&3`) combined with the UUID seed; blur radius at least 2 trace pixels. | `areaShadowF.glsl`, `NV/pipeline.cpp:27016` |
| P0.3 Filter edge-stops | Geometric normal from depth (the `rigRimAuxF` method) for the normal weight. A **plane-distance** depth weight `|dot(p'-p, n_geo)| / max(2 cm, 2·footprint)` replaces the raw dz. | `areaShadowFilterF.glsl`, `screenSpaceRayUtil.glsl` |
| P0.4 Horizon renormalize | Skip disk samples with `dot(n_geo, dir) ≤ 0`; visibility = lit / valid; all below means 1 (shading's N·L owns it) (U2). | `areaShadowF.glsl` |
| P0.5 Geometric offset | Receiver offset and hit refinement/validation use `n_geo`. Validation thickness becomes `min(thickness, max(4·footprint, 5 cm))` for hits with `hit_t < 4·bias`. | `areaShadowF.glsl`, `screenSpaceRayUtil.glsl` |

- **User-visible effect:** penumbrae lose their salt-and-pepper look at 4K,
  Cinematic and K=8. Normal-mapped walls and distant floors stop speckling.
  Avatar terminators clean up. Shadow shapes don't change.
- **Risk:** low-medium. Shader-only plus planner; the ray floor changes which
  light gets shadows under pressure.
- **Effort:** 4-6 days.

**Gates** (new `relight_grain_check.py`, run by both GPU drivers):
- **Penumbra noise.** A straight-edge blocker, flat receiver, analytic
  penumbra. RMS of filtered visibility against a fixed 64-ray/64-step reference
  through the same filter:
  - **≤ 6/255** at Medium 1080p K4;
  - **≤ 8/255** for every preset at 1080p and 4K, K=4 and K=8, Thin OFF and ON,
    at the default budget.
  - HEAD must **fail** the 4K Cinematic case.
- **Speckle.**
  - Lit-region pixels below 250/255 = **0** on flat, grazing (85 degree), curved
    cylinder with a **finite 0.3 m disk**, and normal-mapped (60-degree tilt map)
    receivers.
  - HEAD must fail the curved finite-disk and normal-mapped receivers.
- **Edges.** Shadow does not bleed across a depth discontinuity by more than one
  full-res pixel (an existing-style oracle).
- **Planner.** Literal tables for every preset × 720p/1080p/1440p/4K × K1-8 ×
  Thin ON/OFF:
  - effective rays ≥ 4 or an explicit logged channel drop;
  - rays per pixel are monotonic in the preset;
  - the frame and draw ceilings hold.
- **Determinism and OFF.**
  - Repeated readbacks are byte-identical.
  - Existing contact, fade, VCam, camera and housing regressions keep their
    tolerances.
  - Both lighting masters OFF stay RGBA8-identical to HEAD.
  - Phase 0 needs no new OFF switch: it changes the **ON** path only, as an
    approved quality fix (decision in §4).
- **Mutations that must fail:**
  - restore samples-first degradation (4K noise gate);
  - restore the G-buffer normal in the filter (normal-mapped gate);
  - remove horizon renormalization (curved finite-disk gate);
  - restore per-pixel white-noise seeds (noise gate at Medium);
  - time-seeded rotation (determinism).
- **Cost on the RTX 5090, measured** with Tracy and the existing timestamp
  query: Medium 1080p K4 GPU ms at most 1.15× HEAD. The 4K plans stay within the
  existing iteration ceilings.

### Phase 1: cached cube maps for the top 1-4 point lights, with PCF

- **User-visible effect:** with **World shadows** on, the 1-4 top-ranked point
  lights cast shadows from blockers that are off-screen, behind the camera or
  hidden. They don't swim when the camera moves, and railings near lamps show
  up. The small-kernel PCF is fairly hard, like classic SL shadows. Contact
  detail uses the existing step-2 contact classification at 0.15 m. A
  `World shadows: off / world+contact / world only (compare)` debug selector
  allows in-world A/B against screen space.
- **Scope:**
  - new `NV/alworldshadowmath.h` (pure);
  - new `NV/alworldshadow.{h,cpp}` (atlas owner, face cameras, the cull bucket,
    tile render, sink drain);
  - `CAMERA_LOCAL_SHADOW`;
  - the timing-free sink in `NV/alprobedirty.{h,cpp}`;
  - `pipeline.cpp` hooks (frame start before Prism, gather uniforms, tile-width
    `DEFERRED_SHADOW_TARGET_WIDTH`, `RenderSpotLight` exclusion);
  - new `GLSL/class1/deferred/worldShadowUtil.glsl` attached to
    `gAreaShadowProgram` (`NV/llviewershadermgr.cpp`);
  - reserved uniforms in lockstep (`indra/llrender/llshadermgr.h/.cpp`);
  - settings;
  - one Director row plus a status readout.
- **What P1 deliberately leaves out:**
  - caching is **whole-light** (any event in the sphere invalidates its six
    faces, and any dynamic caster in a needed face re-renders that face every
    frame); the static/dynamic split waits for P3;
  - fixed 512 faces, a 2048² atlas (16 MiB), points only.
- **Risk:** medium. New render pass, cull and state work, and the
  `renderShadow` reuse quirks in §1.2.
- **Effort:** 9-13 days.

**Gates:**
- **Pure tests.**
  - Face matrices round-trip against the six probe look/up vectors.
  - Per-tap face selection is seam-continuous: directions within 1e-4 radians of
    an edge map to texels at most 0.5 texel apart across faces.
  - Face-need, tier and packing are pure functions under all 720 permutations
    of six lights.
  - Invalidation: AABB against face frustum ∩ sphere literals.
- **GPU resolve fixture** (the harness rasterizes analytic boxes into the atlas
  using the production face matrices):
  - **Off-screen blocker:** a blocker behind the camera. World ≤ 0.05 in the
    umbra; screen-only reads 1 (documents the gap).
  - **Hidden blocker:** visibility matches an analytic ray-cast oracle within
    one texel_world at the edge.
  - **Seams:** a blocker straddling every face edge and corner. No gap or step
    greater than 2/255 along the edge.
- **Acne:** the §2.7 acne gate on flat, grazing, curved and normal-mapped
  receivers, after running the tuning procedure. **Peter-panning:** shadow start
  ≤ max(1.5 texel_world, 2 cm) world-only, ≤ 1 full-res pixel with contact.
  **Light leaks:** zero through a 5 cm wall with the light 0.5 m behind it.
- **Thin railing:** a 4 cm rail 3 m from the light, at 512. Darkness ratio
  against the analytic reference in [0.8, 1.2].
- **Camera motion:** eight camera poses over a static scene:
  - the atlas is **byte-identical** across poses (no re-render);
  - the shadow-edge world position on the receiver varies ≤ 0.5 texel_world;
  - pixels seeing the same receiver point agree within 2/255 (world-only mode).
- **Cache correctness:** event sequences (prim move, LOD rebuild, insert, remove,
  alpha texture arrival, region shift, light edit, sink overflow). The cached
  atlas must be **byte-equal** to a cache-bypass render after each event.
  - Mutations that must fail: drop markMoved notes; drop the LOD tag; the
    texture note missing; skip dynamic re-render with a moving avatar box.
- **OFF:**
  - independent-HEAD RGBA8 equality of gather and filter with `area_world=false`
    for every preset, Thin ON and OFF, and mapped fades;
  - **no atlas allocation** (`LLRenderTarget::sBytesAllocated` unchanged);
  - zero extra GL calls in a steady OFF frame (dispatch-fixture counters);
  - no reload or listener.
- **Zero per-frame allocation:** dispatch-fixture counters show zero heap
  allocations and zero GL object creations on steady frames (cache hit,
  dynamic re-render and feed); one allocation on enable or resize.
- **Determinism:** two renders of a fixed frame are byte-identical; a cache
  verify pass matches.
- **Cost on the RTX 5090** (targets, all measured with Tracy):

  | Scene | Budget |
  |---|---|
  | Static, 4 points, all cache hits | ≤ **0.25 ms** GPU at 1080p, ≤ 0.6 ms at 4K; ≤ 0.1 ms CPU |
  | Invalidation frame, 4 × 6 faces at 512 | ≤ 2.5 ms GPU, ≤ 4 ms CPU (one-off) |
  | One walking avatar inside 2 lights | ≤ 1.0 ms GPU + 1.5 ms CPU per frame |

- **Mutations that must fail:** wrong face index; guard band removed with
  per-tap selection off; normal offset along the shaded normal (normal-mapped
  gate); channel-keyed cache (UUID permutation test).
- **In-world:** the same night-alley scene in screen-only, world-only and
  world+contact. Pan and dolly to verify stability. Put a blocker behind the
  camera. Use a railing.

### Phase 2: PCSS softness and light size

- **User-visible effect:** world shadows become soft by the light's real size
  (fixture source size, Size from prim, World light size, 0.10 m fallback). They
  are sharp at contact and widen with distance, and match the screen-space
  penumbra scale at equal settings.
- **Scope:** `worldShadowUtil.glsl` (blocker search, kernel, Vogel 16/32,
  interleaved rotation, per-tap receiver plane), `alworldshadowmath.h` (kernel
  math literals), sampler objects in `alworldshadow.cpp`.
- **Risk:** low-medium.
- **Effort:** 4-6 days.

**Gates:**
- **Contact hardening.** A half-plane blocker at 0.05, 0.5 and 2 m from the
  receiver. The 10-90% penumbra width matches the analytic `r_L(d_r-d_b)/d_b`
  within 15% (blocker inside the cap). It is monotonic in separation and in
  `r_L`. Radius 0 is byte-equal to an explicit 0.10 m radius.
- **Penumbra noise:** ≤ 4/255 RMS against a 256-tap reference.
- **The 5/255 seam rule** holds across cube edges with the largest preset
  kernel.
- **Mutations that must fail:** a constant kernel (hardening gate); time
  rotation (determinism); search radius not divided by `d_r` (width gate);
  missing per-tap plane offset (grazing acne gate).
- **Cost:** ≤ +0.3 ms GPU at 1080p K4 over P1 for cache hits.

### Phase 3: the full atlas, projectors, static/dynamic caching, tiers

- **User-visible effect:**
  - unmapped projectors get world shadows too (no more lost auctions, no
    camera-distance cutoff for them);
  - static scenes cost nearly nothing;
  - lights near the camera get sharper maps;
  - avatar-heavy scenes stay bounded and degrade by rank.
- **Scope:**
  - the 4096² atlas + composite atlas;
  - screen-importance tiers, canonical packing with tile copy;
  - static/dynamic split (copy + overlay);
  - projector tiles through the shared optical-origin helper (native matrices
    byte-unchanged);
  - native-slot policy U3;
  - raster admission and ledger terms in `alareashadowmath.h`;
  - VRAM admission from static inputs;
  - a status readout of resident lights, re-rendered and dynamic faces, MiB, ms.
- **Risk:** medium-high. Cache split correctness, a projector double edge
  against native maps during fades, and the CPU cost of draw submission in
  crowded regions.
- **Effort:** 11-15 days.

**Gates:**
- **Static/dynamic equivalence:** composite tile byte-equal to a single-pass
  render, across 50 random avatar-box placements.
- **Cache bypass:** equal after every event class, including tier changes and
  tile moves.
- **Native refactor:** identical matrices and native map bytes for 100 random
  prim scale/FOV/rotation cases.
- **Projector world tile** against the native map of the same light at 1080p:
  - hard tiny source, no fill, controlled bias;
  - edge separation ≤ 1 full-res pixel;
  - one monotonic boundary (no interior extremum beyond 2/255).
- **Fade continuity:** reuse the 1,001-fade sweep with a resident tile, gather
  ≤ 1/255 per 0.001.
- **Determinism:**
  - packing and tiers are pure under light permutations and camera paths;
  - tier change is logged;
  - with capture pin on, no tier change during a dolly fixture.
- **Memory:**
  - resident bytes equal the plan;
  - zero growth across 1,000 frames of light add/remove churn;
  - release on OFF;
  - admission fails closed to the smaller atlas.
- **Cost on the RTX 5090** (targets):
  - static region with 8 lights: ≤ 0.5 ms GPU at 1080p, ≤ 1.2 ms at 4K;
  - avatar-heavy (20 avatars within 10 m of 4 lights): total world shadow ≤ 4 ms
    GPU and ≤ 4 ms CPU at default budgets, by logged degradation;
  - typical club scene: no frame above the budget.
- **Mutations that must fail:** composite without the static copy (avatar-only
  shadows); stale tile after a tier change; packing not canonical (permutation
  test); a projector tile using the prim centre instead of the apex (edge gate).

### Phase 4: contact hand-off

- **User-visible effect:** no double edges, no peter-panning gaps where feet
  touch floors, and contact detail that stays crisp. The short contact march is
  cheaper than today's full rays, so the grain budget improves for world-owned
  lights.
- **Scope:** short dense contact march for world-owned lights; `d_c` auto
  formula; Thin objects skipped for world-owned lights;
  `RenderAreaShadowMappedContact` reused as the cap.
- **Risk:** medium. Hand-off visual tuning.
- **Effort:** 4-6 days.

**Gates:**
- **Double edge (1080p, full and half trace).** Blockers at 0.02, 0.05, 0.15,
  0.5 and 2 m from the receiver:
  - the hybrid edge profile is monotonic;
  - beyond `d_c` the hybrid is byte-equal to world-only;
  - inside `d_c` the 50% crossing is within 1 full-res pixel of an analytic
    oracle.
- **Peter-panning:** shadow start at the contact line ≤ 1 full-res pixel for a
  box resting on the floor at tiers 256-1024.
- **Thin railing:** passes with Thin objects OFF for world-owned lights.
- **Cost:** contact iterations per world light ≤ 25% of the HEAD Medium march.
- **Mutations that must fail:** a fixed full-length contact (far double-edge
  gate); `d_c` not floored (peter-pan gate); product composition.

### Phase 5: VCam feeds, snapshots, UI, takes

- **User-visible effect:**
  - feeds and Gate sources get world shadows from the shared atlas at almost no
    extra cost;
  - high-res and tiled snapshots keep world shadows;
  - the Director card gets real rows and a status line;
  - scenes save the master;
  - take records explain each shot.
- **Scope:**
  - face-need from registered feed cameras;
  - feed resolve inside allowances;
  - tiled-snapshot full-frustum render + contact off in tiles (U9);
  - `NV/alrelighttakerecord.h` fields;
  - the scene allowlist (`NV/llfloaterdirector.cpp:3743`);
  - kit rows, keywords and help;
  - the three generators, row pin, XUI layout gate.
- **Risk:** medium. Feed ordering and tile seams.
- **Effort:** 6-9 days.

**Gates:**
- **Feeds:** main visibility byte-unchanged with 1, 2 and 4 feeds; feeds never
  render or evict tiles (counter); feed-only light falls back to screen-only.
- **Snapshots:** a 2×2 tiled snapshot of a world-only scene matches the untiled
  render within 2/255 away from the tile borders, with no seam column above
  2/255.
- **Takes:** record round-trip with tier, cache and degradation fields.
- **UI and process:** XUI zero findings; generators `--check`;
  `prebuild_guard -ChecksOnly`.

### Later (not costed in the full scope)

- **P6, alpha receivers:** per-fragment atlas lookup in forward/alpha paths,
  giving hair local shadows (D6). 8-12 days.
- **P7:** probe and hero-mirror captures sample the atlas. 5-8 days.
- **P8:** layered single-pass cube rendering. 5-8 days.

### Effort summary

| Scope | Phases | Engineer-days |
|---|---|---|
| Grain fix only | P0 | 4-6 |
| **Core win** | P0 + P1 + P2 + P4 | **21-31** |
| **Full** | P0-P5 | **38-55** |

The core win gives stable, off-screen-aware, soft, contact-correct shadows for
the 1-4 top-ranked point lights. P3 is what makes them cheap and covers
projectors.

---

## 4. User decisions

| # | Question | Recommendation |
|---|---|---|
| U1 | Under budget pressure: grain (fewer rays) or fewer shadowed lights? | Ray floor 4, then drop the lowest-ranked lights, with a status line. |
| U2 | Should disk rays below the surface horizon count as lit, since shading already handles the horizon? | Yes. It removes the terminator grain on avatars. |
| U3 | A projector holding a native SL slot: A) reuse the SL map plus contact, or B) render our own world map always? | A at first (exact SL look, no §10 change, no double render). Offer B for wide shots: it is deterministic and has no camera-distance cutoff. |
| U4 | Point-light housing: exclude only the emitting prim + near plane, or its whole linkset? | Emitting prim + 0.25 m near plane, so the lamp's own shade still shadows the floor. |
| U5 | Casters: front faces only (like SL), or two-sided? | Front faces. Two-sided stops leaks through single-sided mesh walls but costs acne margin. |
| U6 | Alpha-blend objects (hair, sheer cloth) cast world shadows like native projectors? | Yes, for consistency with SL. |
| U7 | Memory default. | 96 MiB (4096² + composite). Cinematic 8192² opt-in (384 MiB). |
| U8 | Freeze shadow resolution tiers during a take? | Yes. A dolly move then never changes shadow sharpness mid-shot. |
| U9 | Tiled snapshots: world-only shadows (no contact detail) allowed? | Yes. |
| U10 | Save the World shadows master with Director scenes? | Yes, like Soft shadows. Quality and memory stay global. |
| U11 | When "World + contact" becomes the default mode inside Soft shadows. | After the P4 in-world sign-off; until then it is an opt-in row. |
| U12 | Caster LOD in shadow maps: the camera's LOD (consistent with the beauty pass, can pop), or force high LOD within light range (stable, costlier)? | Camera LOD in v1; revisit if popping shows. |

Also: Phase 0 changes the ON path of the shipped Soft shadows, with no new
switch. Confirm that a quality fix may change existing looks, or ask for a
"Classic grain" fallback.

---

## 5. Risks

| Risk | Mitigation |
|---|---|
| **Cost in avatar-heavy scenes.** Each avatar inside a light's range re-renders on every needed face every frame (rigged re-skinning per face). | Face-need culling; dynamic layer only draws movers on a cached copy; `RenderAvatarShadowDetail` and impostor rules; dynamic-face budget with deterministic rank degradation; Tracy gates in P1/P3. Crowded clubs may still demote most lights to screen-only. That is visible, logged and deterministic, but less pretty. |
| **CPU draw submission** (SL is draw-call bound). | One cull per light with `bucketShadowCull`; caching; face budget. Measure CPU separately from GPU. |
| **Stale cache** (the worst failure: a ghost shadow). | Value-compared parameter keys; raw undebounced sink; global invalidation on overflow, teleport and reallocation; byte-equal cache-verify mode; a mutation per event class. Unknown hooks (e.g., a script-driven texture-animation cut-out) still need adding during P1/P3 review. |
| **Light leaks** through thin walls and single-sided meshes. | Normal-offset cap (2 texels / 5 cm); leak fixture; U5. Back-face-culled single-sided walls leak exactly as native SL projectors do. |
| **Peter-panning** from depth/slope bias. | Receiver-plane bias keeps the constant terms small; contact hand-off covers the bias blind band; P4 gates. |
| **Memory and VRAM pressure.** | Static-input admission, fail-closed smaller atlas, release on OFF, counted under the governor's fixed floor. A 384 MiB Cinematic atlas can squeeze texture memory on 8 GB cards: it is opt-in and shown in the status. |
| **Native fades and auction** still vary with time for mapped projectors (B9). | Option U3-B removes this for shots that need full repeatability. |
| **`renderShadow` reuse quirks** (viewport, `sCurResX`, camera id, static cull arrays). | Listed in §1.2; covered by dispatch-fixture ownership and allocation counters. |
| **LOD popping** in shadows when the camera moves. | U12. |
| **Coordination.** P0 overlaps the concurrent Thin objects planner round. | Do not start P0 until that round is merged; re-anchor every line. |

---

## 6. Notes for whoever implements this

- Do not change `BDMergeMaxSpotShadows`, slot allocation, the auction or the
  count-before-filter ordering (§10). U3-B changes only the projector
  **consumer**, and only for atlas-owned lights. It still needs explicit user
  approval.
- Keep the off-screen = lit convention for the **contact** trace. The world map
  is the only source of off-screen occlusion.
- The `shadow_clip.w` projector cutoff finding (§1.1) is a separate,
  pre-existing behaviour. Confirm it in-world before treating it as the cause of
  the 10/21 env-light report.
