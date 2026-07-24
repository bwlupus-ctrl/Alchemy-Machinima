# Research Brief — Native Real-Time Ray Tracing in an OpenGL Second Life Viewer

> **Purpose of this document:** This is a *research brief* to hand to an AI research assistant
> (OpenAI ChatGPT / deep-research / GPT-5-class model). It defines the question, the system under
> study, the sub-questions to investigate, and seed references. The assistant should produce a
> **well-structured, citation-backed research paper with a clear recommendation.** Do the research;
> do not just restate this brief.

---

## 1. Objective (one paragraph)

Determine the best way to add **native, real-time, hardware-accelerated ray tracing** to a large,
legacy **OpenGL** Second Life viewer (an Alchemy/Firestorm-derived viewer with an existing deferred
renderer, PBR/GLTF materials, and reflection probes), on **Windows** targeting **NVIDIA RTX** GPUs
(e.g., RTX 5090). The goal is **real-time in-viewer** rendering (interactive, ~60+ FPS with denoising,
temporal accumulation, and sensible ray budgets) — **not** an offline path tracer, and explicitly
**NOT** a ReShade/screen-space RTGI add-on (that approach is being deliberately avoided). The paper
must **survey every viable integration strategy, compare them, and recommend one** for this specific
codebase, with an implementation roadmap and risk analysis.

## 2. System under study (context the researcher needs)

- **Application:** A third-party **Second Life** viewer derived from the Linden Lab open-source viewer
  (LGPL C++), in the Firestorm/Alchemy lineage. Very large, long-lived C++ codebase.
- **Current renderer:** **OpenGL** (core-profile-ish, historically GL 3.x/4.x). It already has a
  **deferred/G-buffer renderer**, **PBR/GLTF material** support, **reflection probes**, shadow maps,
  and screen-space effects. (Linden's recent "PBR / Love Me Render" work modernized it.)
- **Platform/target:** Windows 11, MSVC (VS2026 toolchain), high-end **NVIDIA RTX** (RTX 5090, 32 GB
  VRAM), Ryzen 9950X, 192 GB RAM. Single-user, high-end target — cross-vendor support is **secondary**
  (NVIDIA-first is acceptable).
- **Threading:** Predominantly a **single-threaded render/main loop** (typical of the SL viewer);
  heavy refactors to multithread the renderer are costly.
- **Use case:** High-fidelity **machinima** (cinematic capture) plus normal interactive use — so
  visual quality of GI/reflections/shadows matters a lot, but it must stay real-time.
- **Content characteristics (critical, see §5):** Fully dynamic, user-generated scenes; **rigged mesh
  avatars**; **animesh** (animated objects driven by client-side control avatars); **Bakes-on-Mesh
  (BOM)/baked avatar textures** that mutate at runtime; **streamed mesh with multiple LODs**; objects
  constantly created/destroyed/moved; no offline bake step or authored light baking.
- **What's being replaced/avoided:** A prior **ReShade RTGI** (screen-space, Pascal-Gilcher-style)
  approach was tried and is being abandoned (screen-space limitations, integration friction). We want
  **true world-space RT** integrated into the client.

## 3. The core question & scope

**Primary question:** *What is the best architecture to integrate real-time hardware ray tracing into
this OpenGL viewer, and how would we implement it?*

Scope constraints to respect throughout:
- **Real-time in-viewer** performance is the priority (interactive frame rates with denoising +
  temporal accumulation + ray budgets). Not an offline renderer.
- **Survey ALL integration strategies and recommend** the best (see §4).
- **No ReShade / no pure screen-space RTGI** as the solution.
- NVIDIA RTX-first is acceptable; note where a choice sacrifices cross-vendor portability.
- Must be realistic about a **legacy, largely single-threaded OpenGL C++ codebase**.

## 4. Integration strategies to compare (be exhaustive & comparative)

For **each** strategy: how it works, what it requires in the codebase, performance ceiling, engineering
effort (S/M/L/XL), risk, portability, and how RT results feed back into the existing **deferred**
pipeline.

1. **Keep OpenGL + hardware RT via interop.** Investigate all three interop routes:
   - **Vulkan RT interop** — drive `VK_KHR_ray_tracing_pipeline` and/or `VK_KHR_ray_query` from a
     Vulkan device that shares resources with the GL context via **`GL_EXT_external_objects` /
     `GL_EXT_external_objects_win32`** (shared memory objects, textures, and semaphores). Cover
     building/sharing BVH (acceleration structures), sharing G-buffer/depth/normal textures, and
     synchronization (timeline/binary semaphores) between GL and Vulkan.
   - **NVIDIA OptiX** interop with OpenGL (CUDA-GL resource sharing).
   - **CUDA + OpenGL interop** for custom RT kernels (`cudaGraphicsGLRegisterImage/Buffer`).
   Discuss the cost of GL↔RT resource sharing, cross-API sync overhead, and driver/platform caveats
   on Windows/NVIDIA.

2. **Compute-shader / software ray tracing in OpenGL (no RT cores).** SDF/ray-marched tracing,
   **voxel cone tracing (VXGI)**, **DDGI**-style irradiance-probe GI, world+screen-space hybrids.
   When is this competitive vs. hardware RT? What quality/perf ceiling on dynamic scenes with no
   pre-bake?

3. **Migrate the render backend to Vulkan** to use **native `VK_KHR_ray_tracing` / ray query**
   directly (no interop layer). Weigh the cost of a Vulkan port of a large legacy GL renderer against
   the long-term payoff; discuss partial/incremental migration and whether a GL→Vulkan translation
   layer (e.g., Zink-like) is relevant.

## 5. Second-Life-specific technical constraints (do not skip — these decide feasibility)

Explain how each of these interacts with real-time RT, and what it forces in the design:
- **Dynamic acceleration structures:** avatars and **animesh** deform every frame (skinning); objects
  spawn/despawn/move constantly. Cover **BLAS/TLAS build vs. refit**, per-frame AS update budgets,
  skinned-geometry handling (where do post-skinning vertices come from for the BVH?), and instancing.
- **Baked/BOM & mutable textures:** avatar **Bakes-on-Mesh** and other textures change at runtime;
  how does that affect RT material binding, texture residency, and any shared descriptor tables?
- **Mesh LOD streaming:** meshes arrive asynchronously and swap LODs by distance; how does the AS/
  RT layer cope with geometry that is not fully resident and changes LOD?
- **No authored/baked lighting:** everything is dynamic UGC; light probes must be **fully runtime**
  (favoring DDGI/ReSTIR-style dynamic techniques over precomputed GI).
- **Legacy single-threaded pipeline:** AS builds and RT dispatch competing with the main thread;
  async compute / separate queues; realistic threading changes required.
- **Material model:** existing **PBR/GLTF** materials + legacy Blinn-Phong; how RT shading reuses or
  duplicates the material/shader system.

## 6. Effects, denoising, budgets (real-time feasibility)

- **Which hybrid RT effects** fit a real-time budget and their relative cost/benefit: **RT reflections,
  RT shadows (incl. soft/area), RTGI (DDGI/ReSTIR GI), RTAO.** Recommend a phased subset.
- **Sampling/robustness:** **ReSTIR / ReSTIR GI**, spatiotemporal reservoir resampling; 1-spp-class
  pipelines.
- **Denoising:** **NVIDIA Real-Time Denoisers (NRD / ReBLUR / ReLAX / SIGMA)**, **OptiX AI denoiser**,
  **SVGF/A-SVGF**, and plain temporal accumulation. Trade-offs and integration into a deferred GL/interop
  pipeline.
- **Ray budgets & upscaling:** rays-per-pixel targets; **DLSS / DLSS Ray Reconstruction** and temporal
  upsampling to hit frame budget at high resolution; how upscaling interacts with denoising.

## 7. Real-world precedents to analyze

- **Quake II RTX / Q2VKPT** (Vulkan path tracing of a legacy game) — architecture, AS management,
  denoising, lessons.
- Other **open-source Vulkan ray-tracing** ports/samples (e.g., NVIDIA `vk_raytracing_tutorial_KHR`,
  RTXGI/DDGI SDK samples, RTXDI/ReSTIR samples).
- Any **OpenGL→RT interop** examples or engines that added RT to a GL renderer.
- Engines' hybrid RT (e.g., how UE-style hybrid RT or "RTX Remix"-style approaches structure GI/
  reflections) — for architecture patterns, not as drop-in.
- Prior art of **RT in virtual-world/UGC/dynamic-scene** contexts specifically (closest analogues).

## 8. Required output (what the paper must contain)

1. **Executive summary** + a **clear recommendation** (which of §4's strategies, and why) with a
   confidence level.
2. **Comparison matrix** of the strategies across: perf ceiling, effort, risk, portability, code
   disruption, quality.
3. **Recommended architecture** — data flow from GL geometry → AS build → RT dispatch → denoise →
   composite back into the deferred pipeline; where interop/sync happens.
4. **Phased implementation roadmap** (e.g., Phase 0 spike/interop proof → RT shadows → RT reflections →
   RTGI → denoise/upscale), each phase with scope and a "prove-it" milestone.
5. **Handling of the SL-specific constraints in §5** — concrete answers, not hand-waving.
6. **Risk register** and open questions.
7. **REFERENCES** — a strong, organized, citation-backed list (see §9).

## 9. Reference seeds & search terms (the researcher should verify and expand these)

Use these as *starting points* — fetch primary sources, confirm current API/version details, and add more.

**Specs / APIs**
- Khronos: `VK_KHR_ray_tracing_pipeline`, `VK_KHR_ray_query`, `VK_KHR_acceleration_structure`,
  `VK_KHR_deferred_host_operations`; OpenGL `GL_EXT_external_objects`, `GL_EXT_external_objects_win32`,
  `GL_EXT_semaphore`; the (older/experimental) `GL_NV_ray_tracing` and `GLSL_NV_ray_tracing`.
- NVIDIA **OptiX** programming guide; **CUDA–OpenGL interop** docs (`cudaGraphicsGLRegister*`).

**SDKs / tools**
- NVIDIA **NRD** (Real-Time Denoisers: ReBLUR/ReLAX/SIGMA), **RTXGI/DDGI** SDK, **RTXDI** (ReSTIR DI),
  **DLSS** SDK incl. **Ray Reconstruction**, **Nsight Graphics** for profiling.

**Papers / techniques**
- **DDGI** — Majercik et al., "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields."
- **ReSTIR** — Bitterli et al. 2020 (DI); **ReSTIR GI** — Ouyang et al. 2021.
- **SVGF / A-SVGF** — Schied et al. (spatiotemporal variance-guided filtering).
- **Voxel Cone Tracing** — Crassin et al.
- Quake II RTX / **Q2VKPT** technical write-ups (Christoph Schied).

**Real-world / engineering**
- NVIDIA `vk_raytracing_tutorial_KHR`; "Ray Tracing Gems" I & II (relevant chapters); vendor blogs on
  hybrid RT, AS build/refit strategies, and skinned-geometry BVH updates.

**Search terms to run**
- "OpenGL Vulkan interop ray tracing EXT_external_objects", "VK_KHR_ray_query deferred renderer hybrid",
  "acceleration structure refit skinned mesh per frame", "DDGI dynamic scene no bake", "ReSTIR GI
  real time 1 spp", "NRD ReBLUR integration", "Quake II RTX architecture denoiser", "DLSS ray
  reconstruction integration", "OptiX OpenGL interop denoiser real time".

## 10. Success criteria

A strong answer: picks a defensible strategy for *this* codebase and justifies it against the
alternatives; is concrete about GL↔RT interop and per-frame AS updates for skinned/animesh/streamed
content; gives a realistic phased roadmap and effort/risk; and cites primary sources (specs, SDK docs,
papers, real ports) that a follow-up deep-research pass can build on.
