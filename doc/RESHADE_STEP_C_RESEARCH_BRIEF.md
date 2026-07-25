# Research brief — ReShade "Step C": real G-buffer semantics into iMMERSE, and retiring Launchpad

**Audience:** ChatGPT deep research
**Project:** `bwlupus-ctrl/Alchemy-Machinima` (fork of Alchemy Viewer / Linden Lab Second Life
viewer), branch `develop`. C++ / **OpenGL** desktop app. Also relevant: the sibling Firestorm fork
`phoenix-reshade-XL` at `I:\enve`.
**Goal:** stop iMMERSE effects (RTGI/MXAO/RCAO/DOF/TAA) from *guessing* scene data, feed them the
viewer's real deferred G-buffer, and then **disable Marty Friedmann's Launchpad entirely** to
reclaim the compute it spends reconstructing data we already have.

---

## 1. What already exists (verified against the source — build on this, don't redesign it)

### 1.1 Architecture: a published ABI struct + an external add-on
The viewer does **not** link the ReShade add-on SDK. Instead:

- `indra/newview/llreshadebridge.{h,cpp}` fills and publishes a C struct, `SLReShadeFrame`, once per
  frame from `LLReShadeBridge::gatherFrame()`.
- `indra/newview/llreshadebridgeabi.h` is the **shared contract**.
- A separate add-on binary, `sl_reshade_bridge.addon` (source tree `reshade-addon/`), reads that
  struct and binds the textures into ReShade texture semantics.
- The add-on is **byte-for-byte interchangeable** between Alchemy-Machinima and phoenix-reshade-XL;
  only the viewer side differs.

Relevant history on `develop`: `a7db2d78aa4` (Step A, add-on + depth), `e0d822edcbd` (Step B, motion),
`d3a3092962e` (R32F depth copy), `54a0fef95f7` (bind via `create_resource_view`), `9cf220b04c9`
(clamp `GL_TEXTURE_MAX_LEVEL`), `5b947d485e5` (shelved), `259c7a5453e` (current bridge).

### 1.2 What the viewer ALREADY publishes each frame
From `gatherFrame()` and the `deferredScreen` attachment layout
(`0 = albedo/diffuse, 1 = ORM (occlusion/roughness/metal), 2 = normals, 3 = emissive`):

| ABI field | Source | Notes |
|---|---|---|
| `color_hdr` | screen target | HDR flag in `SLRESHADE_FLAG_HDR` |
| `depth` | shared depth (`GL_DEPTH_COMPONENT24`) | ABI marks it *informational*; `generic_depth` is the intended depth path for effects |
| `albedo` | `deferredScreen` attachment 0 | **the real, unlit base colour** |
| `orm` | attachment 1 | occlusion / roughness / metal |
| `normals` | attachment 2 | **spheremap-encoded in `.xy`** — decode per `decodeNormal` in `globalF.glsl` |
| `emissive` | attachment 3 (optional) | |
| `motion` | `gPipeline.mVelocityMap` | **Alchemy only** (the A5.4 velocity subsystem); phoenix-reshade-XL leaves this zero |

Each `SLReShadeTexture` carries `{gl_name, gl_internal_format, width, height}`; `gl_name == 0` means
"not available this frame". GL names are **not stable across frames** — the ABI requires re-reading
and comparing `{gl_name, width, height, gl_internal_format}` before reuse. Published textures are
**level-0 only** (hence the `GL_TEXTURE_MAX_LEVEL` clamp fix — GL mipmap-incompleteness previously
made bound textures sample flat/grey).

### 1.3 The binding mechanism that actually works (hard-won)
Hand-encoding a ReShade `resource_view` handle **does not work** for sampling — the view is not
registered, so ReShade does not know its format and every semantic sampler reads flat/zero. The
correct sequence (mirroring ReShade's own `generic_depth` add-on) is:

```
device* dev = runtime->get_device();
resource res = { (GL_TEXTURE_2D << 40) | glName };
dev->create_resource_view(res, resource_usage::shader_resource, resource_view_desc(fmt), &srv);
runtime->update_texture_bindings(semantic, srv, srv);
```

Bind **once per handle**, not per frame; re-create the view only when the GL name or size changes.
This detail is make-or-break for every buffer.

### 1.4 Where Step B stopped
Step B publishes real motion vectors and an original, IP-clean `SL_GBufferProvider.fx` writes them
into iMMERSE's **shared** `Deferred::MotionVectorsTex` (same `namespace`/name/size/format, so ReShade
shares the resource). The quality win is real — but **Launchpad still runs its optical-flow pass**;
we merely overwrite its output. **There is no compute offload yet.** That is what Step C is for.

---

## 2. The research questions

### Q1 — Normals: spheremap → octahedral, exactly
SL stores **spheremap-encoded** normals in `deferredScreen` attachment 2 `.xy`, decoded by
`decodeNormal()` in `globalF.glsl`. iMMERSE's Launchpad publishes `Deferred::NormalsTexV3`, which
(per Marty's shared-texture convention) is **octahedral-encoded**.

- What exactly is `NormalsTexV3`'s format, encoding, packing and **coordinate space**?
  Our working assumption is that SL's deferred normals are **view-space**, matching Marty's
  convention — **verify this**, because a space mismatch silently ruins GI/AO.
- Give the correct **decode-then-re-encode** transform (never a bit-copy) from SL spheremap `.xy` to
  octahedral, including handedness, Y/Z orientation and any range/bias differences.
- How should we handle sky/background pixels, and faces with no valid normal?
- What precision is required (RG16F? RGBA8?) to avoid banding in GI?

### Q2 — ALBEDO ⭐ **THE PRIMARY FOCUS — this was tried and it FAILED**

**Known failure, from in-world testing:** the viewer's albedo **was** streamed across to ReShade and
it **caused visual glitches**. This is the single biggest open problem in the whole effort, and the
most valuable thing this research can solve. Please treat it as the headline question rather than
one item among many.

Launchpad *reconstructs* albedo by attempting to **de-light the final image** — an approximation and
a known artifact source. SL is a deferred renderer, so `deferredScreen` attachment 0 is already the
real, unlit albedo, and the bridge publishes it. In principle this should be strictly better. In
practice it glitched. **Why?**

Candidate causes worth investigating rigorously (this list is a starting point, not a limit):

1. **sRGB vs linear mismatch.** What is attachment 0's actual GL internal format — `GL_RGBA8`,
   `GL_SRGB8_ALPHA8`, something else? If the texture is sRGB and ReShade's registered
   `resource_view_desc(fmt)` declares a linear format (or vice versa), the hardware applies or skips
   the sRGB transfer function and every colour is wrong — often subtly, exactly like "glitches".
   Note the binding path explicitly passes a format to `create_resource_view`; getting that wrong is
   silent.
2. **Attachment 0 may not be pure albedo.** Determine precisely what SL packs there across material
   types — legacy Blinn-Phong vs GLTF/PBR vs fullbright vs emissive — and what the **alpha channel**
   means (it may carry an environment/material flag rather than opacity). Marty's consumer may
   interpret alpha differently.
3. **Pixels with no G-buffer albedo.** Alpha-blended geometry, water, sky/background and the UI never
   write the deferred albedo attachment. Those pixels contain stale or uninitialised data. Launchpad's
   de-lit reconstruction produces *something* everywhere; a real G-buffer does not. This alone could
   read as glitching, especially around transparent objects and against the sky.
4. **Validity window.** Is attachment 0 still intact at the moment `gatherFrame()` publishes, or has a
   later pass (glow, post, tonemap, or a subsequent deferred pass reusing the target) already
   overwritten it? SL reuses render targets aggressively.
5. **Resolution / TAAU mismatch.** With `_MARTYSMODS_TAAU_SCALE = 0.66`, GI runs at reduced
   resolution while the published G-buffer is full-resolution. How does iMMERSE expect
   `Deferred::AlbedoTex` to be scaled, and is it sampling with the wrong texel scale?
6. **Expected content/range.** Does Marty's pipeline expect albedo *post*-tonemap or in a particular
   range/space because it was derived from the displayed image? Feeding true linear scene albedo may
   violate an implicit assumption in downstream effects.

**What we need back:** a definite diagnosis path — how to distinguish these causes from each other
in-world (what each failure mode *looks like*: colour shift vs banding vs halos vs sky garbage vs
ghosting), the correct format/space/transform to publish, how to handle pixels with no deferred
albedo, and whether any of this requires changing what the viewer writes rather than what the add-on
binds.

Related, secondary: how do PBR/GLTF, legacy, and fullbright/emissive materials differ in attachment
0, and are we *worse* than Launchpad's guess for alpha-blended pixels specifically?

### Q3 — Which Launchpad passes can then be switched off?
This is the actual performance goal. For each Launchpad responsibility — optical-flow motion
vectors, normal reconstruction from depth, albedo de-lighting, and anything else it publishes —
determine:
- what it publishes and under what semantic/name;
- whether providing the real buffer makes that pass genuinely redundant;
- **how to disable it** (technique toggles, preprocessor definitions, or simply not loading
  Launchpad) without breaking iMMERSE effects that expect its resources to exist;
- what the expected GPU-time saving is, and what breaks if a semantic is missing rather than
  merely wrong.

### Q4 — Ordering and lifetime
- Correct ReShade **technique order** once the provider supplies everything (currently
  Launchpad → SL G-Buffer Provider → RTGI).
- Where in the SL frame is the G-buffer valid to publish? `gatherFrame()` runs from
  `llviewerdisplay`; confirm the deferred attachments are complete and not yet overwritten by later
  passes (glow, post, tonemap).
- Behaviour on **resolution change, HDR toggle, and render-target reallocation**, given GL names are
  unstable across frames.
- Interaction with **TAAU**: `_MARTYSMODS_TAAU_SCALE` was previously undefined, so GI ran at full 4K;
  it is now set to `0.66`. How does a scaled TAAU interact with externally provided full-resolution
  G-buffer semantics?

### Q5 — The IP boundary (hard constraint)
Marty's iMMERSE/Launchpad shaders are **proprietary**. The rule for this project: *provide data into
documented shared texture names; never read, modify, redistribute or derive from his shader source.*
Confirm what is legitimate here — publishing into a shared `namespace Deferred` texture that his
shaders declare, and shipping our own original `.fx` — and flag anything in the proposed approach
that would cross that line.

---

## 3. Deliverable
A design document that a competent C++/GLSL engineer can implement from:
1. the exact normal transcode (with formulas), and how to verify it is right;
2. a verdict on albedo — direct passthrough or specified transform;
3. a concrete list of Launchpad passes to disable and how;
4. technique ordering, validity window and resolution/HDR handling;
5. a **verification plan** — how to prove each semantic is correct in-world (debug visualizations,
   expected failure signatures when encoding/space is wrong) rather than "looks about right";
6. explicit statements of what could not be determined from public information.

## 4. Sources worth consulting
- ReShade add-on API docs and the reference `generic_depth` add-on
  (`examples/09-depth/generic_depth_addon.cpp`) — the authoritative pattern for registering views
  and `update_texture_bindings`.
- ReShade's OpenGL backend type conversion (`opengl_impl_type_convert.*`) — note this project
  maintains a **fork** of ReShade at `I:\reshade-SL` which already carries a 10-bit
  (`R10G10B10A2`) detection fix that was upstreamed as a PR candidate.
- Marty McFly / Pascal Gilcher iMMERSE public documentation and shared-texture conventions
  (`Deferred::NormalsTexV3`, `Deferred::MotionVectorsTex`, `Deferred::AlbedoTex`), plus any public
  material on Launchpad's responsibilities.
- Second Life viewer deferred pipeline: `indra/newview/pipeline.cpp` (`addDeferredAttachments`),
  `indra/newview/app_settings/shaders/class1/deferred/globalF.glsl` (`decodeNormal`), and the
  GLTF/PBR material path.
- Standard references on octahedral vs spheremap normal encoding (e.g. Cigolle et al., "A Survey of
  Efficient Representations for Independent Unit Vectors") for the transcode math.
- In-repo context: `doc/SL_TRUTH_RESHADE_BRIEF.md`, and the archived full-bridge work on branch
  `archive/reshade-rtgi-bridge` / tag `reshade-rtgi-bridge-archive`.
