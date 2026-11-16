# Volume Player — animated smoke & fire from Blender (VDB) in the viewer — design brief

**Date:** 2026-11-16 (rev 2, after adversarial review round 1)
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Review:** round 1 (independent Opus) found 4 must-fix, 12 should-fix and 10 nits; all applied in rev
2. Claude re-verified the depth-buffer and alpha-channel findings in the code. The rev-2 edits aren't
re-reviewed; the code-review loop must attack §5.4 (depth copy, composite and blend state) first.
**Process (per `CLAUDE.md`):** Codex implements (`--write`, `--prompt-file`), Opus reviews the code, loop
to 0 must-fix, Claude builds once, the user tests in-world. Codex never builds.

> **BASE.** `file:line` references are from branch `ccr-221def9f-h17t1m` at `89520b4`, which contains
> `origin/fix/animesh-clone-pose-polish`. Re-anchor against any unpushed local work.

---

## 1. The ask

> "Is SL able to play vdb files like in Blender?" … "smoke and fire, animated sequences, build a doc
> for codex."

Play **animated smoke and fire simulations made in Blender** (Mantaflow bakes, stored as OpenVDB `.vdb`,
one per frame) **inside the viewer**:

- placed in the world;
- lit consistently with the scene;
- fire glows (including bloom);
- playback stays in step with the take (Temporal Capture).

**Client-only:** only this viewer sees it, like Local Mesh and Local Animation. Second Life has no
volume asset type, and this feature never uploads anything.

---

## 2. Decision D1 — how `.vdb` gets in (read first; it changes the build)

The viewer has **no OpenVDB support** today (no `openvdb`, `tbb` or `blosc` in `indra/vcpkg.json`; no
mention in the source).

| | **A. Read `.vdb` directly** | **B. Blender exporter → viewer format (default)** |
|---|---|---|
| User flow | Point the viewer at the Blender cache folder | One extra step in Blender: run the bundled exporter, then load the result |
| New dependencies | vcpkg `openvdb` (pulls TBB, Blosc, Boost pieces, Imath) on all three platforms | **None.** Blender does the VDB reading; the viewer reader uses zlib, already a dependency (`indra/vcpkg.json:48`) |
| Build risk | High (a large dependency set, possible Boost clash) | Low |
| Load speed | Decode and resample at play time | Pre-resampled, quantised frames: inflate and upload |

**Default: B.** A is a later option (§10). **Codex must not add OpenVDB to vcpkg without the user
choosing A.**

---

## 3. What the code PROVES today

| Fact | Where |
|---|---|
| 3D textures are supported. **There is no 1D texture type**, so use an N×1 2D texture for LUTs. | `llrender/llrender.h:74-81` |
| A colour LUT is already uploaded with `glTexImage3D`. A **hardened** 3D loader (bounds-checked reads, `glGetError` drain, delete on failure → effect off) exists for the projector dust volume: **the precedent.** | `newview/pipeline.cpp:2769-2778`; `:2800-2950` |
| `glTexStorage3D` is **null below GL 4.2** (macOS is 4.1). `glTexSubImage3D` is loaded. So allocate once with `glTexImage3D(nullptr)`, then update with `glTexSubImage3D`. | `llrender/llgl.cpp:2192-2206`, `:1712` |
| `LLImageGLMemory::alloc_tex_image(w, h, fmt, count, mips)` records base bytes × `count`. It asserts **unit 0** and reads the texture bound through `gGL`. `GL_RGB8` is counted at 24 bpp, though drivers usually pad it to 32 (inference). | `llrender/llimagegl.h:51-63`; `llimagegl.cpp:134, 146-151, 480` |
| **VRAM** headroom: `LLViewerTexture::sFreeVRAMMegabytes`. `BDMergeMemoryBudget` is **system RAM** (physical/commit/process), not VRAM. | `newview/llviewertexture.cpp:680`; `newview/bdmergememorybudget.h:14-19` |
| The thread pool's `General` queue defaults to **1 thread**, shared with GLTF material decode. | `app_settings/settings.xml:23511`; `newview/llgltfmateriallist.cpp:555` |
| `renderFinalize` asserts `!gCubeSnapshot`, its only caller is `render_ui`, and the Prism aux view never calls it. So it runs for the **main view only**: mirrors and VCam feeds will not show volumes. | `pipeline.cpp:21731`; `llviewerdisplay.cpp:1632`; `llprismlens.cpp:7589` |
| `renderFinalize` captures matrices at entry ("post passes alter GL state"). Post passes use `get_current_modelview()` / `get_current_projection()`. It **disables `GL_CULL_FACE`**. | `pipeline.cpp:21741`, `:20155`, `:21789` |
| The volumetric composite point: luminance/exposure and bloom generation, then `renderFroxelVolumetrics`, then `renderProjectorVolumetric`, then the bloom feed (`feedProjectorVolumetricBloom`), then `colorCorrect`. All on linear HDR `mRT->screen`. | `pipeline.cpp:21827-21829`, `:21880`, `:21894-21906`, `:21927` |
| **The scene depth at that point is NOT opaque-only.** Rigged alpha (hair) always writes depth, pre-water alpha writes depth, and with `RenderDepthOfField` on a final pass writes all post-water alpha with alpha ≥ 0.33 (glass, particles) to depth. | `newview/lldrawpoolalpha.cpp:289-307`, `:321-330`; depth shared at `pipeline.cpp:1323` |
| **The scene alpha channel carries prim glow** to the legacy glow extract on the non-HDR path, so a composite must not write alpha. The froxel apply's blend: `blendFunc(BF_ONE, BF_SOURCE_ALPHA)`, `setColorMask(true, false)`, restored after. **Copy it.** | `postDeferredGammaCorrect.glsl:65`; `glowExtractF.glsl:61`; `pipeline.cpp:19978-20007` |
| The froxel apply composites `L_fog + scene·T_fog` against scene depth; the integrated froxel volume (`mFroxelIntegrated`) is available when `mFroxelIntegratedValid`. | `pipeline.cpp:19371-19390`, `:19973-20007` |
| The scene's sun and ambient go through `calcAtmosphericVarsLinear`: sRGB→linear sunlight × `sky_sunlight_scale` / `sunlight_hdr_scale` × EnvIntensity sun EV × tint, and grey ambient × ambient EV × tint, with auto-adjust. Raw `getSunlightColor()` / `getTotalAmbient()` skip all of that. | `class1/windlight/atmosphericsFuncs.glsl:201-243`; `llsettingsvo.cpp:965-1023`; `llsettingssky.h:243, 314` |
| The Ghost Studio motion clock: a once-per-generation advance by presentation delta while Temporal Capture drives a feature, else wall delta. The precedent for the volume clock. | `newview/alghoststudio.cpp:1915-1929` |
| Local fog stores **agent-space** `LLVector3` centres, which break after a region crossing. Don't copy that; store `LLVector3d` global. | `newview/allocalfogmanager.h:39` |
| Debug and UI overlays are drawn gated on UI visibility and `RENDER_DEBUG_FEATURE_UI` (`render_actor_path_guides`). | `pipeline.cpp:21745-21747` |

---

## 4. The file format — `.alvol` (route B)

Versioned and little-endian. **Every field is validated before use.**

```
Header:
  magic             "ALVOL\0\0\1"         8 bytes
  version           u32 = 1
  header_size       u32                   bytes, including the per-channel ranges
  table_entry_size  u32                   bytes per frame-table entry (>= v1 size; extra bytes skipped)
  flags             u32                   bit0 sqrt-companded density (exporter default ON)
  channels          u32                   bit0 density, bit1 flame; v1 requires density (flame optional)
  dims              u32 x, y, z           each 8..256
  frame_count       u32                   1..4096
  fps               f32                   1..120
  fire_kelvin_min   f32, fire_kelvin_max f32  flame 0..1 -> colour temperature (1000..4000 K, min < max)
  ranges            f32 min,max per present channel, in bit order (count = popcount(channels))
Frame table (frame_count entries, table_entry_size each):
  offset            u64
  size              u32                   compressed bytes
  bbox_min          f32 x3, bbox_max f32 x3   this frame's grid box, metres, in the volume's local frame
  emission_total    f32
  emission_centroid f32 x3                emission-weighted centre, local frame
  emission_kelvin   f32                   emission-weighted mean colour temperature
Frame chunk: zlib stream -> dense u8, 2 bytes per voxel (density, flame; flame 0 if absent), x fastest
```

**The reader rejects:**

- a bad magic or a version other than 1;
- `header_size` or `table_entry_size` smaller than v1's;
- dims, channels or flags out of range;
- `range.min ≥ max`, `fire_kelvin_min ≥ max`, or any non-finite float;
- `offset[0] <` end of table, `offset[i] + size[i] > offset[i+1]`, or `offset + size > file size`;
- a bbox with min ≥ max on any axis.

**Inflate rules:** inflate into a **fixed buffer of exactly** `x·y·z·2` bytes (computed in u64; the max
is 256³·2 = 32 MiB) and require `Z_STREAM_END` with exactly that output.

**Quantisation:** u8 with **sqrt companding** of density by default (flag bit0), because integration
along rays does not remove quantisation contours, and wisps below 1/255 of the range would vanish
linearly. A u16 density is a future flag.

### 4.1 The exporter (`scripts/blender/export_alvol.py`, a Blender add-on or text-editor script)

**Input:** the fluid domain object, a frame range, a target resolution (max axis, default 128).

**Reading the grids**, in order of preference:

1. **Blender's own fluid API** for the current frame: `FluidDomainSettings` `density_grid` and
   `flame_grid` (stepping the frame). Inference: no extra module needed. Verify on the user's Blender
   version.
2. **Blender's bundled OpenVDB Python module** (`openvdb` or `pyopenvdb` — try both) on the cache's
   `.vdb` files.

If neither works, fail with a clear message naming both.

**Coordinates:**

- Honour each frame's **grid transform and the domain object's world matrix**. Mantaflow adaptive
  domains move and resize per frame.
- Write **a per-frame bbox** in the frame table (not a union box, which wastes resolution).
- The volume's local frame is the domain's local frame at the first frame.
- **Array order:** OpenVDB `copyToArray` fills `[i, j, k]`, so z is fastest. **Transpose to x-fastest**
  for GL.

**Fire colour:** `heat` is Mantaflow's buoyancy temperature, **not** fire colour, so never alias it.
Colour comes from `flame` (0..1), mapped to `fire_kelvin_min..max` (defaults 1000 K / 3500 K; also read
from the domain's flame settings if exposed).

**Also emitted:** the per-frame emission stats, and a log line per frame.

**Test generator:** `scripts/blender/make_test_alvol.py` (plain Python 3, no Blender, no numpy) writes a
procedural puff/flame sequence, plus a checker that re-reads and validates it.

---

## 5. Runtime design

### 5.1 Module and ownership

`ALVolumePlayer` (singleton, `newview/alvolumeplayer.{h,cpp}`) owns up to **4 instances**. Each instance
has:

- the file, the parsed header and the frame table;
- placement: **global** position (`LLVector3d`), rotation, uniform scale, and an optional **attach**
  (object or avatar joint, re-read each frame). A culled or impostored avatar's joints stop updating, so
  an attached volume holds its last pose; that's stated.
- playback: play/pause, loop / once / ping-pong, speed, start offset, frame blend;
- look: density scale, smoke albedo, absorption, anisotropy `g`, fire EV, ambient-match gain;
- a quality preset.

**Pure maths in `newview/alvolumemath.h`** (header-only, TUT): header and frame-table validation, frame
index from time, ray/box, the Henyey-Greenstein phase, the blackbody LUT generator, sqrt-companding
decode.

### 5.2 Clock and take sync

- Per instance, advanced once per presentation generation, like `advanceMotionClock`: by presentation
  delta while Temporal Capture drives **PARTICLES or ANIMATION** (smoke and fire are effects, so follow
  either; verify the enum names in `lltemporalframecontext.h`), else wall delta.
- **Restart all** button, plus an optional **restart on capture start**, so a take always begins at the
  sequence's frame 0 plus its offset.
- Frame position = `offset + clock · fps · speed`, mapped by loop mode. **Frame blend** samples frames ⌊p⌋
  and ⌊p⌋+1 and mixes them by the fraction, so slow motion is smooth.

### 5.3 Streaming and memory

- **Decode thread:** a **dedicated, bounded** worker (one thread, a queue of at most `RingSlots`
  frames). **Not** the shared `General` queue (1 thread, shared with material decode).
  - Read files with `LLFile::fopen` off-thread, as the dust loader does.
  - Inflate into a CPU ring.
  - Under `BDMergeMemoryBudget::isCritical()` (system RAM), shrink the CPU ring to its minimum and log it.
- **GPU ring:** `RingSlots` 3D textures per instance (default 8; **minimum 4 with frame blend on**, so a
  slot sampled last frame is never rewritten this frame).
  - **Format: `GL_RG8`** (density, flame), always 2 channels.
  - Allocate each slot once: `bindManual(TT_TEXTURE_3D)` on **unit 0**, then `glTexImage3D(nullptr)`,
    then `alloc_tex_image(x, y, GL_RG8, z)`. Free with `LLImageGL::deleteTextures`, and unbind every
    `TT_TEXTURE_3D` unit afterwards.
- **Uploads** (main thread): `glTexSubImage3D` into a free slot, **capped by bytes per frame**
  (`VolumeUploadBytesPerFrame`, default 8 MiB). Save and restore `GL_UNPACK_ALIGNMENT`, set to 1 because
  rows are 2·x bytes.
- **VRAM budget:** the GPU ring and preload key off `LLViewerTexture::sFreeVRAMMegabytes`.
  - **Preload for recording** decodes and uploads every frame only if the total ≤ `PreloadVramFraction`
    (default 50%) of free VRAM. Otherwise refuse, stating the bytes needed and available.
  - **Warn in the UI** that a large preload makes the texture governor discard world textures, because the
    3D textures are accounted against the same budget.
- **Late frame:** hold the nearest resident earlier frame, never stall. Log `[Volume] late frames N` per
  second.
- **Sizes**, to show in the UI: 128³ × RG8 = 4 MiB per frame; 8 slots = 32 MiB; a 240-frame preload =
  960 MiB.

### 5.4 Rendering

**1. Opaque depth copy** (new, gated). The scene depth at the composite point includes depth-writing
alpha, and changes with DoF (§3). So:

- At the end of `renderDeferredLighting` (before post-deferred alpha), **only when at least one volume
  instance is visible**, copy the deferred depth into a new `mVolumeOpaqueDepth` target (a depth blit or
  a fullscreen copy).
- That gives smoke a stable, predictable occluder: opaque geometry only.
- Shared-pipeline change: it must be fully inert when off (no allocation, no blit), and the review must
  check FBO, draw-buffer and depth state restoration (`CLAUDE.md` rule 1).

**2. Sun-transmittance volume** (per instance; rebuilt only when the frame or the sun direction changes
by more than ~0.5°):

- A low-resolution 3D texture (64³, R8) of transmittance toward the sun.
- Computed by rendering each slice with `glFramebufferTextureLayer` and a short march through the
  density.
- The per-pixel cost then becomes one fetch instead of a nested march (the reviewer's arithmetic: about
  3,200 fetches per pixel otherwise).

**3. The pass `LLPipeline::renderVolumePlayer(LLRenderTarget* target)`**:

- **Where:** called in `renderFinalize` **after `renderProjectorVolumetric`** (`pipeline.cpp:21896`) and
  **before the bloom feed**, so fire can feed bloom (§5.5).
- **Gate:** run only when `mRT == &mMainRT && !sPrismLensRender`, the feature is on, and an instance is
  visible. Otherwise return with no allocation and no state change.
- **Matrices:** `get_current_modelview()` / `get_current_projection()`, not the `gGL` stack.
- **Coverage:** a **fullscreen triangle with the scissor set to the box's projected screen rectangle**.
  Culling stays off (no `glCullFace` change). Enable scissor explicitly with a scoped guard and restore
  its rectangle afterwards (the projector pass changes it).
- **March** (half-resolution target `mVolumeHalf`, `RGBA16F`, a new target, not the projector's):
  - the ray is intersected with the box in local space, from entry to `min(exit, mVolumeOpaqueDepth)`;
  - **step** = voxel size × step scale, capped at MaxSteps (64 / 96 / 160);
  - **jitter:** spatial only (a hash of integer `gl_FragCoord`), so re-renders are identical;
  - **at each sample:**
    - `σ = density_scale · decode(d)`;
    - sun = `sunlight` × phase(`g`) × sun-transmittance;
    - ambient = `amblit` × `ambient_match` × a local occlusion estimate;
    - emission = `fire_ev` × flame × blackbody LUT (an N×1 2D texture);
    - accumulate front to back, early-out at T < 0.01.
  - **Sun and ambient come from `calcAtmosphericVarsLinear`**, so the program is built as an atmospherics
    shader with the same includes and uniforms as the deferred passes. They therefore follow the sky,
    HDR scale, EnvIntensity and tints exactly as surfaces do. Probe irradiance is **not** sampled in v1,
    so smoke may not exactly match probe-lit surfaces next to it; the ambient-match gain compensates
    (L6).
  - **Fog and haze at the smoke:** when `mFroxelIntegratedValid`, attenuate and add the integrated
    froxel fog **at the smoke's front depth**, and apply windlight haze (atten/additive) at that depth,
    so distant smoke reads hazed like everything else.
- **Upsample:** depth-aware bilateral upsample (against `mVolumeOpaqueDepth`).
- **Composite:** `scene = scene·T + L`, using **exactly the froxel apply's state**:
  `blendFunc(BF_ONE, BF_SOURCE_ALPHA)`, `setColorMask(true, false)` (never write scene alpha; it carries
  prim glow on the non-HDR path), then restore `BT_ALPHA` and `setColorMask(true, true)`.
- **Multiple instances:** composite back to front by box centre. Interpenetration is approximate (L4).

### 5.5 Fire bloom (Phase 1)

"Fire glows" is part of the ask. After compositing, feed a scaled copy of the **fire emission only** into
the bloom pyramid base, mirroring `feedProjectorVolumetricBloom` (`pipeline.cpp:21906`), gated by a
`FireBloom` gain (default on). HDR path only.

### 5.6 Interactions and exclusions

- **Main view only** (§3): mirrors, reflection probes and VCam/Prism feeds show no volumes.
- **Alpha in front of smoke** (hair, glass, particles) is drawn **under** the smoke (L1).
- **Depth of field** uses scene depth, so smoke blurs with what's behind it (L2).
- **Exposure:** the composite runs after luminance metering, so a frame full of fire won't change
  exposure. **Use manual EV** for fire-heavy shots (stated in the UI tooltip).
- **On-lens filters and the ReShade raw capture** run before this pass, so smoke and fire **bypass**
  Graduated ND, Night Mask and the bridge's raw capture (L7).
- **Non-HDR path:** there's no tonemap, so bright fire clips flat. Fire EV is clamped to keep emission
  ≤ 1 on that path; documented.
- **S-Log3:** linear HDR fire; the S-Log3 colour-stage brief's shoulder handles hot cores.

---

## 6. UI — "Volume Player" floater (Me menu, and the Director Console's World group)

- **List:** Load… (`.alvol`), Duplicate, Remove, visibility.
- **Placement:**
  - numeric global position, rotation and scale;
  - Place at camera focus / at my avatar's feet / Attach to selected object (joint picker for avatars);
  - a **bounding-box gizmo drawn in the UI overlay** (gated like `render_actor_path_guides`), never into
    `mRT->screen`.
- **Playback:** Play/Pause, Restart, **Restart all**, Restart on capture start, Loop mode, Speed, Start
  offset, Frame blend, **Preload for recording** (memory readout and governor warning).
- **Look:** density, smoke colour, absorption, anisotropy, fire EV, fire bloom, ambient match.
- **Quality:** a preset, plus readouts of resident and late frames and GPU ms.
- Per-control reset buttons. **Prove there's no overlap or clipping by rect arithmetic.**
- **Persistence:** a saved bank (file path, **global** placement, playback, look). A missing file loads
  as a disabled entry with a warning.

---

## 7. Settings (`settings_alchemy.xml`)

| Key | Default | Notes |
|---|---|---|
| `AlchemyVolumePlayerEnabled` | true | master gate |
| `AlchemyVolumeRingSlots` | 8 | 4–64 |
| `AlchemyVolumeUploadBytesPerFrame` | 8388608 | 1–64 MiB |
| `AlchemyVolumePreloadVramFraction` | 0.5 | 0.1–0.8 |
| `AlchemyVolumeMaxInstances` | 4 | hard cap |
| `AlchemyVolumeHalfRes` | true | |
| `AlchemyVolumeVerify` | true | periodic verdicts |

---

## 8. Instrumentation (`[Volume]`)

Each item ends in a PASS, FAIL or REFUSED verdict.

- **Load:** `loaded <file>: 128x96x128, 240 frames @24 fps, density+flame, sqrt, 4.0 MiB/frame -> PASS`.
  On failure, `FAIL: <field>=<value> (<rule>)`.
- **Per second while playing:** resident / ring, late frames, upload bytes, sun-volume rebuilds, march
  GPU ms (Tracy zone plus a timer query).
- **Off path:** `[Volume] inactive: no depth copy, no pass` once when the last instance is hidden.
- **Determinism harness:** the same frame twice at a fixed resolution, diffed. PASS or FAIL.
- **Preload:** `preloaded 240/240, 960 MiB of 2400 MiB free -> PASS`, or `REFUSED: needs X, allowed Y`.

---

## 9. Tests

**TUT (`newview/tests/alvolumemath_test.cpp`).** Literals; each test must fail if broken.

- **Validation:** every reject rule in §4, a valid header passing, and `header_size` and
  `table_entry_size` larger than v1 (forward-compatible) being accepted. A fuzz loop of byte flips never
  crashes and never accepts an out-of-bounds or overlapping offset.
- **Frame index:** loop, once and ping-pong at speeds 1, 0.5 and 2; a negative or NaN clock gives frame 0;
  the blend fraction is in [0, 1); ping-pong never repeats an end frame twice in a row.
- **Ray/box:** outside, inside, grazing, parallel, behind; never NaN.
- **HG phase:** integrates to 1; `g = 0` is isotropic.
- **Blackbody LUT:** 1000 K is red-dominant; 3500 K is warmer than 6500 K; monotonic blue growth.
- **Companding:** decode(encode(x)) within one code across [0, 1]; 0 and 1 exact.

**Exporter.** `make_test_alvol.py` plus its checker (no Blender). In Blender, a 10-frame Mantaflow fire
test exports with the transposed axis order verified by an asymmetric test shape. A plume rising along
+Z must rise along +Z in the viewer.

**In-world, outcomes stated in advance:**

1. Load the test puff. It appears at the camera focus, animates, and loops cleanly. Load PASS.
2. Camera inside the volume: it renders (fullscreen coverage), with no box-face clipping.
3. A wall through the volume: smoke stops at the wall. **A window pane in front of the smoke does not cut
   it** (opaque-depth copy), and toggling DoF doesn't change the smoke.
4. Prim glow on a nearby object is unchanged with smoke on and off (alpha channel untouched).
5. Temporal Capture at 0.25×: the fire slows to quarter speed, and frame blend keeps it smooth.
6. Preload a 240-frame bake, then record: 0 late frames.
7. Feature off or no instances: the image diff against the baseline is identical, and the log says no
   depth copy and no pass.
8. A Blender fire bake exported with the tool: glowing core with bloom, smoke rising above it, lit by the
   sun **with the same tint and strength as nearby surfaces**, hazed at distance like the scene.

---

## 10. Phasing (render-state feature: slicing allowed, A/B between steps)

| Phase | Delivers |
|---|---|
| **1** | Format, exporter, test generator; validated reader; dedicated decode thread; GPU ring and preload; clock and take sync; placement, attach and UI; opaque-depth copy; sun-transmittance volume; half-res march with atmospherics-consistent sun and ambient, fog and haze, fire emission; composite; **fire bloom feed**; bank; verdicts; TUT. A complete smoke-and-fire player. |
| **2** | Fire lights the scene: one client-only point light from the frame table's emission centroid, Kelvin and total, using the rig's local-emitter pattern and gate exemptions. Local lights on smoke; the sun shadow map on smoke (first verify it's still valid at the composite point); probe irradiance for ambient. |
| **3** | Alpha-aware compositing; volumes in VCam/mirror views; smoke casting shadows; route A (direct `.vdb`) if wanted. |

---

## 11. Known limitations (stated, not hidden)

- **L1 — Alpha:** hair, glass and particles in front of smoke are drawn under it.
- **L2 — Depth of field:** smoke blurs with the background behind it.
- **L3 — Shadows:** smoke casts no shadow on the scene.
- **L4 — Overlaps:** interpenetrating volumes composite approximately.
- **L5 — Resolution:** a dense 128³ grid loses the finest wisps; export at up to 256³ for close-ups (8×
  the memory).
- **L6 — Ambient:** smoke may not exactly match probe-lit surfaces until Phase 2 samples probes; use the
  ambient-match gain.
- **L7 — On-lens filters:** smoke bypasses Graduated ND, Night Mask and the ReShade raw capture.
- **L8 — Views:** main view only (no mirrors, probes or VCam feeds).
- **L9 — Client-only:** other people never see it.

---

## 12. Off-limits

- **No OpenVDB / TBB / Blosc in vcpkg** unless the user picks route A.
- **G-buffer layout and the ReShade bridge ABI.**
- **Existing volumetric passes** (froxel, projector shafts, local fog) are read-only precedents. The only
  shared-pipeline additions are the gated depth copy and the new pass call.
- **Shared GL state:** every state touched (blend, colour mask, scissor and its rectangle, depth test,
  FBO and draw buffers, unpack alignment, texture units) is set explicitly and restored. Review it against
  the beauty pass and the off path.
- Never build. Claude builds after review convergence.

---

## 13. Open decisions (defaults chosen)

- **D1 — Route:** B. A later.
- **D2 — Export resolution:** 128 on the longest axis.
- **D3 — Instances:** at most 4.
- **D4 — Frame blend:** on.
- **D5 — Floater location:** the Me menu plus the Director World group.

---

## Appendix — review round 1 disposition (applied in rev 2)

| # | Finding | Disposition |
|---|---|---|
| M1 | Scene depth includes depth-writing alpha and changes with DoF | Gated opaque-depth copy; L1 rewritten; test 3 |
| M2 | Composite must not write scene alpha (prim glow) | Exact froxel blend and colour-mask pattern |
| M3 | Back-face coverage needs `glCullFace`, which leaks | Fullscreen triangle, scissored; culling untouched |
| M4 | CPU sun/ambient won't match the scene | Atmospherics shader with `calcAtmosphericVarsLinear`; L6 |
| S1 | Composite order vs froxel fog, projector shafts and haze | After shafts; fog and haze at the smoke front |
| S2 | Nested light march too costly | Sun-transmittance volume |
| S3 | RAM vs VRAM budgets confused | `sFreeVRAMMegabytes` for GPU; `isCritical` for CPU; preload cap and warning |
| S4 | The `General` queue is 1 thread and shared | Dedicated bounded decode thread |
| S5 | Upload cap by count; alignment; 3-slot reuse | Byte cap; `UNPACK_ALIGNMENT` 1 saved and restored; minimum 4 slots |
| S6 | Channel and format inconsistency | `RG8` always |
| S7 | Format growth and validation holes | `header_size`, `table_entry_size`, `flags`, overlap and inflate rules |
| S8 | u8 rationale wrong | sqrt companding by default |
| S9 | Exporter: axis order, `heat` ≠ fire temperature, transforms, fallback | Transposed; flame→Kelvin; per-frame bbox and transforms; fluid API first |
| S10 | Agent-space placement; attach freeze | Global `LLVector3d`; freeze stated |
| S11 | Fire glow deferred against the ask | Bloom feed in Phase 1 |
| S12 | No take-start anchor; drive bit | Restart all / on capture start; PARTICLES or ANIMATION |
| Nits | Matrices, no 1D texture, accounting, no `TexStorage`, new half-res target, filter bypass, exposure, dead exclusion flags, gizmo, non-HDR | All applied |
