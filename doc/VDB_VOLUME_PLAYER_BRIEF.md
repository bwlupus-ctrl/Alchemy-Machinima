# Volume Player — animated smoke & fire from Blender (VDB) in the viewer — design brief

**Date:** 2026-11-16
**Status:** DESIGN ONLY. Nothing implemented, nothing built, **not yet adversarially reviewed**.
**Process (per `CLAUDE.md`):** an Opus adversarial review of this brief first; then Codex implements
(`--write`, `--prompt-file`), Opus reviews the code, loop to 0 must-fix, Claude builds once, the user
tests in-world. Codex never builds.

> **BASE.** `file:line` references are from branch `ccr-221def9f-h17t1m` at `89520b4`, which contains
> `origin/fix/animesh-clone-pose-polish`. Re-anchor against any unpushed local work.

---

## 1. The ask

> "Is SL able to play vdb files like in Blender?" … "smoke and fire, animated sequences, build a doc
> for codex."

Play **animated smoke and fire simulations made in Blender** (Mantaflow bakes, which Blender stores as
OpenVDB `.vdb` files, one per frame) **inside the viewer**:

- placed in the world;
- lit by the scene;
- fire glows;
- playback stays in step with the take (Temporal Capture).

**Client-only:** only this viewer sees it, like Local Mesh and Local Animation. Second Life has no
volume asset type, and this feature never uploads anything.

---

## 2. Decision D1 — how `.vdb` gets in (read first; it changes the build)

The viewer has **no OpenVDB support** today (no `openvdb`, `tbb` or `blosc` in `indra/vcpkg.json`; no
mention in the source). Two routes:

| | **A. Read `.vdb` directly** | **B. Blender exporter → viewer format (recommended for v1)** |
|---|---|---|
| User flow | Point the viewer at the Blender cache folder | One extra click in Blender: run the bundled exporter on the cache, then load the result |
| New dependencies | vcpkg `openvdb` (pulls TBB, Blosc, Boost pieces, Imath) on Windows, macOS and Linux | **None.** The exporter runs inside Blender, using Blender's bundled OpenVDB Python module; the viewer reader uses zlib, which is already a dependency (`indra/vcpkg.json:48`) |
| Build risk | High: a large C++ dependency set, possible Boost version clash with the viewer's own | Low |
| Load speed | Each frame decoded and resampled at play time | Frames pre-resampled and quantised once, so the viewer just inflates and uploads |
| Fidelity | Full sparse VDB | Dense grid at a chosen resolution (default 128³), which is plenty for the camera distances in SL |

**Default: B.** A is listed as a later option (§10). **Codex must not add OpenVDB to vcpkg without the
user choosing A.**

**Verify item for B:** the exact name of Blender's bundled OpenVDB Python module in the user's Blender
version (`pyopenvdb` in older releases, `openvdb` in newer ones). The exporter must try both and fail
with a clear message if neither imports.

---

## 3. What the code PROVES today

| Fact | Where |
|---|---|
| 3D textures are supported by the render layer. | `llrender/llrender.h:79` (`TT_TEXTURE_3D`), `llrender/llrender.cpp:88, 491` |
| A colour-grading LUT is already uploaded as a 3D texture with `glTexImage3D`. | `newview/pipeline.cpp:2769-2778` |
| A **hardened** 3D-texture loader exists for the projector-shaft dust volume: bounds-checked file reads, `glGetError` after upload, delete on any failure, so it degrades to "effect off", never a partial texture. **The precedent to copy.** | `pipeline.cpp:2800-2950` (`loadProjVolDustMap`) |
| Volumetrics composite onto the **linear HDR scene buffer in `renderFinalize`**: after bloom generation, before `colorCorrect` (tonemap), additive in place on `mRT->screen`. Froxel volumetrics then projector shafts run here. | `pipeline.cpp:21884-21896` |
| The froxel pass's master-gate pattern: off, or a cube-map capture, or the program not compiled → no allocation, no passes. | `pipeline.cpp:19371-19390` |
| Local fog volumes: up to 8 analytic box/ellipsoid volumes, a saved bank, a `tick(presentation_time)`, and injection into the coarse froxel grid. Too coarse for detailed smoke, but the UI and bank precedent. | `newview/allocalfogmanager.h` |
| Local assets loaded from disk, client-only, with a registry, a scroll-list feed and live reload: Local Mesh and Local Animation. | `newview/lllocalmesh.h:209-350`, `newview/lllocalanim.h` |
| VRAM accounting for raw GL textures: `LLImageGLMemory::alloc_tex_image / free_tex_image`, which feeds the budget governor. Memory pressure: `BDMergeMemoryBudget::isCritical()`. | `llrender/llimagegl.h:51-63`; `newview/bdmergememorybudget.h` |
| The presentation clock, and the rule for following Temporal Capture: advance by the presentation delta only while `drives(LLTemporalFeature::ANIMATION)`, otherwise by wall delta (the Ghost Studio motion clock, `ALGhostStudio::advanceMotionClock`). | `newview/llpresentationtime.h`; `newview/alghoststudio.cpp` |
| Kelvin → linear RGB (Planckian) maths already exists in the light rig model; reuse its method for the fire colour LUT. | `newview/alcinelightrigmodel.cpp` (`planckianLinearRGB`) |

---

## 4. The file format — `.alvol` (route B)

A small, versioned, little-endian container. **Every field is validated before use** (copy the dust
loader's discipline).

```
Header (fixed):
  magic        "ALVOL\0\0\1"         8 bytes
  version      u32 = 1
  dims         u32 x, y, z           each 8..256
  channels     u32 bitmask           bit0 density, bit1 emission (flame), bit2 temperature
  frame_count  u32                   1..4096
  fps          f32                   1..120 (the Blender scene fps)
  voxel_size   f32 metres            > 0, finite
  bbox_min     f32 x3 metres         object space of the bake
  per-channel range  f32 min,max     the dequantisation range for each present channel
Frame table (frame_count entries):
  offset       u64                   absolute file offset of the frame chunk
  size         u32                   compressed byte size
  emission_total     f32             sum of emission (used by Phase 2's fire light; CPU, no readback)
  emission_centroid  f32 x3          emission-weighted centre, object space
  emission_kelvin    f32             emission-weighted mean colour temperature
Frame chunk:
  zlib stream → dense u8 array, channel-interleaved, x fastest
```

- The reader rejects:
  - a bad magic or a version other than 1;
  - out-of-range dims or channels;
  - offsets or sizes outside the file;
  - a frame table that isn't strictly increasing;
  - a decompressed size ≠ `x·y·z·channel_count`;
  - any non-finite float.
- **u8 quantisation:** smoke density banding at u8 is acceptable because density is integrated along
  rays. The exporter applies a per-channel range and optional sqrt companding (header flag — reserve one
  in `channels` bit 8).

### 4.1 The exporter (`scripts/blender/export_alvol.py`, a Blender add-on or text-editor script)

- Input: the domain object with a Mantaflow cache (or a folder of `.vdb` files), a frame range, a target
  resolution (max axis, default 128) and channels.
- For each frame:
  - read the grids `density`, `flame` and `temperature` (and `heat` as an alias), via Blender's OpenVDB
    Python module;
  - resample to the target dense grid (trilinear) across the **union bounding box of the whole
    sequence**, so the grid doesn't jump between frames;
  - quantise, deflate and write;
  - compute the emission stats for the frame table.
- Output: one `.alvol` file plus a log line per frame.
- **The test asset generator** (`scripts/blender/make_test_alvol.py`, plain Python 3, no Blender needed)
  writes a procedural puff/flame sequence. This lets the viewer side be built and tested without
  Blender.

---

## 5. Runtime design

### 5.1 Module and ownership

`ALVolumePlayer` (singleton, `newview/alvolumeplayer.{h,cpp}`) owns up to **4 instances**. Each instance
has:

- the source file, the parsed header and the frame table;
- placement: world position, rotation, uniform scale, and an optional **attach** to an object or avatar
  joint (follows it each frame);
- playback: play/pause, loop / once / ping-pong, speed, start offset, frame blend on/off;
- look: density scale, smoke albedo colour, absorption, anisotropy `g`, fire EV, fire temperature
  remap, emission colour mode (blackbody or tint);
- quality: preset, which sets resolution scale, steps and light steps.

**Pure maths in `newview/alvolumemath.h`** (header-only, TUT): header validation, frame index from time,
the ray/box intersection, the Henyey-Greenstein phase, the blackbody LUT generator and dequantisation.

### 5.2 Clock

- Each instance keeps its own clock, advanced exactly like `ALGhostStudio::advanceMotionClock`: by
  presentation delta while Temporal Capture drives ANIMATION, else wall delta. So slow motion slows the
  fire.
- Frame position = `start_offset + clock·fps·speed`, mapped by loop mode. **With frame blend on**, the
  shader samples frames ⌊p⌋ and ⌊p⌋+1 and mixes them by the fraction. That keeps slow motion smooth
  instead of steppy, at the cost of a second 3D texture fetch per sample.

### 5.3 Streaming and memory

- **Worker thread** (the viewer's thread pool, as other decode work uses): inflate frames ahead into a CPU
  ring of decoded frames.
- **Main thread:** upload at most `VolumeUploadsPerFrame` (default 2) into a **GPU ring** of `R` 3D
  textures (default 8 slots, minimum 3 when frame blend is on). Use `glTexSubImage3D` into existing
  storage, never re-allocating per frame.
- **Accounting:** every 3D texture is registered with `LLImageGLMemory::alloc_tex_image` / `free_tex_image`
  so the VRAM governor sees it.
- **Under `BDMergeMemoryBudget::isCritical()`:** shrink the ring to its minimum and log it.
- **Late frame** (not resident when needed): hold the nearest resident earlier frame. Log
  `[Volume] late frame N (held M)` once per second with counts. Never stall the main thread.
- **Preload for recording:** a per-instance button decodes and uploads **all** frames if they fit the
  budget, so a captured take has no late frames. Refuse with a clear message if it doesn't fit, giving
  the bytes needed and the bytes available.
- **Sizes**, to show in the UI: 128³ × 2 channels × u8 = 4 MiB per frame on the GPU. 8 slots = 32 MiB.
  A 240-frame preload at 128³ = 960 MiB.

### 5.4 Rendering (the new pass)

**Where:** a new `LLPipeline::renderVolumePlayer(LLRenderTarget* target)`, called in `renderFinalize`
**immediately before `renderFroxelVolumetrics`** (`pipeline.cpp:21894`). Same composite point: linear
HDR, pre-tonemap, after bloom generation (so fire doesn't skew auto-exposure; see the bloom note in
§5.6).

**Gate:** feature off, no visible instance, `gCubeSnapshot`, or the program not compiled → return with
no allocation, no state change. The froxel gate pattern.

**Algorithm per instance:**

1. **Coverage:** rasterise the instance's oriented box, back faces, so it works with the camera inside
   it, into a **half-resolution** target (`RGBA16F`: in-scattered light rgb + transmittance a).
2. **Ray:** for each pixel, intersect the view ray with the box in object space (pure function). March
   from entry to `min(exit, opaque scene depth)`.
   - The step is `voxel_size · step_scale`, with a hard cap of `MaxSteps` (preset: 64 / 96 / 160).
   - **Jitter** the start by a **spatial** blue-noise / hash value of integer `gl_FragCoord` only. No
     frame counter and no time, so re-renders of a frame are identical (the determinism rule from
     `doc/CINEMATIC_SSGI_DESIGN.md` §6).
3. **At each sample:**
   - density `σ = density_scale · d`, extinction `σt = σ · (absorption + scattering)`;
   - **sun:** colour and direction from the current sky (CPU, as uniforms) × the HG phase(`g`) ×
     **self-shadow**, from a short march toward the sun (`LightSteps`: 4 / 6 / 10) through the same volume;
   - **ambient:** the sky ambient colour (CPU, `getTotalAmbient`-equivalent) × a cheap occlusion
     estimate from the local density;
   - **emission:** `fire_ev` × emission × blackbody(temperature) from a 1D LUT texture (1000–3500 K,
     built on the CPU with the rig's Planckian method), or a tint;
   - accumulate front-to-back. Early-out when transmittance < 0.01.
4. **Upsample:** depth-aware bilateral upsample to full resolution (the same approach as the projector
   volumetrics' half-resolution march).
5. **Composite:** `scene = scene · T + L`. **The blend state must be explicitly set and restored**
   (`CLAUDE.md` rule 1: never inherit blend state).

**Instances overlap:** v1 renders instances in back-to-front order of box centre and composites each.
Interpenetrating volumes are approximate (stated, L4).

### 5.5 Lighting in Phase 2 (stated now, not built in Phase 1)

- **Fire lights the scene:** one client-only point light per fire instance. It sits at the frame
  table's emission centroid; its colour comes from `emission_kelvin` and its intensity from
  `emission_total`, with a user gain. **CPU data, so no GPU readback.**
  - Use the light rig's local-emitter pattern (`LLVOVolume` made client-side) and its gate exemptions.
- **Local lights on smoke:** the nearest 4 local lights as uniforms, unshadowed.
- **Sun shadow map on smoke:** sample the sun cascades at sample points, so smoke in a building's shadow
  goes dark. First verify the shadow maps are still valid at the `renderFinalize` composite point
  (inference).

### 5.6 Interactions and exclusions (v1)

- **Views:** only the **main view** gets volumes. Excluded: reflection-probe captures and hero mirrors
  (`gCubeSnapshot`), Prism/VCam feeds (`sPrismLensRender`), HUDs, impostors, water reflection (an
  explicit predicate like the Relight brief's).
- **Alpha** (hair, glass, particles) is drawn before the composite point, so **smoke is drawn over alpha
  that is in front of it** (L1). The same limitation the projector shafts have today.
- **Depth of field** uses opaque depth, so smoke blurs with what's behind it (L2).
- **Bloom:** fire doesn't feed bloom in v1, because the bloom pyramid is generated before this pass. An
  optional "fire glow" feed is a Phase 2 item, mirroring `feedProjectorVolumetricBloom`.
- **Exposure:** fire is not metered (composited after luminance), so it won't pump auto-exposure.
  Intended.
- **S-Log3 capture:** fire values are linear HDR; the S-Log3 colour-stage brief's shoulder handles hot
  fire cores.
- **ReShade bridge:** volumes are not in the G-buffer, so RTGI and other bridge effects don't see them.
- **Shadows cast by smoke onto the scene:** not in v1 (L3).

---

## 6. UI — "Volume Player" floater

Opened from the Me menu (next to Lightbox) and the Director Console's World group.

- **List:** loaded instances, with Load… (file picker for `.alvol`), Duplicate, Remove, and a visibility
  check.
- **Placement:**
  - numeric position, rotation and scale;
  - **Place at camera focus**, **Place at my avatar's feet**, **Attach to selected object** (with a joint
    picker for avatars);
  - a bounding-box gizmo while the floater is open (display-only; never drawn in captures).
- **Playback:** Play/Pause, Restart, Loop mode, Speed, Start offset, Frame blend, Preload for recording
  (with a memory readout).
- **Look:** density, smoke colour, absorption, anisotropy, fire EV, fire temperature min/max, emission
  mode.
- **Quality:** a preset (Low / Medium / High / Cinematic). Live readouts: resident frames, late frames,
  GPU ms.
- Reset buttons per control (the weather-panel idiom). **Prove there's no overlap or clipping by rect
  arithmetic.**
- **Persistence:** a saved bank (file path, placement, playback and look per instance) like the
  local-fog bank. A missing file loads as a disabled entry with a warning, never a crash.

---

## 7. Settings (`settings_alchemy.xml`)

| Key | Default | Notes |
|---|---|---|
| `AlchemyVolumePlayerEnabled` | true | master gate (no instances = no cost anyway) |
| `AlchemyVolumeRingSlots` | 8 | 3–64; GPU frames resident per instance |
| `AlchemyVolumeUploadsPerFrame` | 2 | 1–8 |
| `AlchemyVolumeMaxInstances` | 4 | hard cap |
| `AlchemyVolumeHalfRes` | true | half-resolution march |
| `AlchemyVolumeVerify` | true | periodic verdict logging (§8) |

---

## 8. Instrumentation — the log states a verdict (`[Volume]`)

- **Load:** `loaded <file>: 128x96x128, 240 frames @24 fps, channels density+flame, 4.0 MiB/frame ->
  PASS`. Otherwise `FAIL: <reason>`, naming the exact field and value that failed validation.
- **Per second, while playing:** resident / ring, late frames, uploads, upload ms, march GPU ms (a Tracy
  zone plus a timer query), and a memory-critical flag.
- **Determinism harness:** render the same frame twice at a fixed resolution and diff. `PASS` or `FAIL`.
- **Preload:** `preloaded 240/240 frames, 960 MiB -> PASS`, or `REFUSED: needs X, budget Y`.

---

## 9. Tests

**TUT (`newview/tests/alvolumemath_test.cpp`, registered like the other header-only tests).** Use
literals; each test must fail if the thing it tests is broken.

- **Header validation:** every reject case in §4, plus a valid minimal header that passes. A fuzz loop
  over random byte flips never crashes and never accepts an out-of-bounds offset.
- **Frame index:**
  - loop, once and ping-pong at speed 1, 0.5 and 2;
  - a negative or NaN clock gives frame 0;
  - the frame-blend fraction is in [0, 1);
  - ping-pong never repeats an end frame twice in a row.
- **Ray/box:** camera outside, inside, grazing, parallel to a face, behind; a non-finite input never
  produces NaN.
- **HG phase:** integrates to 1 over the sphere; `g = 0` is isotropic; the sign of `g` favours
  forward/back.
- **Blackbody LUT:** 1000 K is red-dominant; 6500 K is near-white with green normalised; monotonic blue
  growth with temperature.
- **Dequantisation:** endpoints exact; the sqrt companding round-trips within one code.

**Exporter.** `make_test_alvol.py` writes a known sequence, and a Python checker re-reads it,
validating the header and the frame table. Run without Blender in CI-like conditions.

**In-world, outcomes stated in advance:**

1. Load the test puff. It appears at the camera focus, animates, and loops cleanly. The log shows load
   PASS.
2. Walk into it: no clipping at the box faces (back-face coverage), and the scene behind is visible
   through thin smoke.
3. An opaque wall through the volume: smoke stops at the wall (depth test).
4. Temporal Capture at 0.25×: the fire slows to quarter speed; frame blend on keeps it smooth.
5. Preload a 240-frame bake, then record: 0 late frames in the log.
6. Feature off, or no instances: an image diff against the baseline is identical (prove the off path is
   inert).
7. A Blender Mantaflow fire bake, exported with the exporter, reads as fire: glowing core, smoke above,
   lit by the sun.

---

## 10. Phasing (render-state feature: slicing allowed, A/B between steps)

| Phase | Delivers |
|---|---|
| **1** | Format + exporter + test generator; reader with validation; streaming and preload; clock; placement, attach and UI; the half-resolution raymarch with sun, self-shadow, ambient and fire emission; composite; bank; verdicts; TUT. **A complete, usable smoke-and-fire player.** |
| **2** | Fire lights the scene; local lights on smoke; sun shadow map on smoke; fire bloom feed. |
| **3** | Alpha-aware compositing; VCam/probe views; smoke casting shadows; route A (direct `.vdb`) if the user wants it. |

---

## 11. Known limitations (stated, not hidden)

- **L1 — Alpha:** smoke draws over alpha geometry (hair, glass, particles) that is in front of it.
- **L2 — Depth of field:** DoF treats smoke as part of the background.
- **L3 — Shadows:** smoke casts no shadow onto the scene.
- **L4 — Overlapping volumes:** interpenetrating volumes composite approximately (sorted by box
  centre).
- **L5 — Resolution:** a dense 128³ grid loses the finest wisps of a high-resolution bake; raise the
  export resolution (up to 256³, at 8× the memory) for close-ups.
- **L6 — Client-only:** other people never see it.

---

## 12. Off-limits for the implementer

- **No OpenVDB / TBB / Blosc in vcpkg** unless the user picks route A.
- **G-buffer layout, the ReShade bridge ABI, existing volumetric passes:** froxel, projector shafts and
  local fog are read-only precedents.
- **Shared GL state:** every state the new pass touches is set explicitly and restored. Review it against
  the beauty pass and the feature-off path.
- Never build. Claude builds after review convergence.

---

## 13. Open decisions (defaults chosen)

- **D1 — Route:** B (Blender exporter). A later.
- **D2 — Default export resolution:** 128 on the longest axis.
- **D3 — Instance cap:** 4.
- **D4 — Frame blend default:** on.
- **D5 — Where the floater lives:** the Me menu plus the Director World group.
