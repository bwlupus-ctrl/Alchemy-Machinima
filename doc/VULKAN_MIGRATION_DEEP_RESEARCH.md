# OpenGL → Vulkan Migration — Deep-Research Brief (for GPT)

**Fork:** Alchemy-Machinima (a Second Life viewer fork, OpenGL renderer), `I:\alchemy-machinima`.
**Type:** RESEARCH + architecture paper. Produce a thorough, source-grounded analysis and a phased strategy — **not** a code patch, and **do NOT build**. Where concrete, include skeleton code (an RHI interface sketch, a one-pass Vulkan backend outline), but the deliverable is a paper. Ground every claim about the current renderer in the actual tree (cite file:line); ground every Vulkan claim in real spec/best-practice with citations.

**Framing directive (important):** do NOT write a cheerleading "how to port to Vulkan" doc. Write a **critical feasibility study**: analyze the migration honestly, including whether it is worth it **for this fork specifically** (a machinima-focused fork whose primary target is a single high-end Windows/NVIDIA machine — see [[target-machine-specs]]), and present the strongest case *against* doing it alongside the case for.

---

## 0. Goal + motivation (state both sides)
Why anyone migrates the SL renderer to Vulkan:
- **Longevity / platform:** OpenGL is deprecated on macOS (max GL 4.1; Apple pushes Metal) — the Mac viewer already leans on the aging GL path; Vulkan-via-**MoltenVK** is the modern route. GL driver quality is also stagnating on some vendors.
- **Driver-overhead + threading:** Vulkan's explicit, multithreaded command submission can cut CPU draw-call overhead — relevant to SL's notoriously draw-call-heavy scenes (many objects, avatars, attachments).
- **Modern GPU features:** first-class compute, descriptor indexing / bindless, timeline semaphores, dynamic rendering, mesh shaders, explicit memory control.
- **Perf headroom** for the fork's heavy deferred/BD-merge pipeline.

Why it may NOT be worth it (research + argue):
- The SL renderer is one of the most **GL-coupled** large C++ codebases in the wild (immediate-mode remnants, global GL state, ~20 years of GL assumptions). A faithful port is a **multi-year, multi-engineer** effort even for Linden Lab.
- For a machinima fork on a 5090, the bottleneck is rarely CPU draw-call overhead; the wins may not justify the risk of a huge regression surface.
- The fork's differentiators (ReShade bridge, custom deferred passes) are GL-specific and would need reinvention.

Deliver a clear verdict: **full port / partial (macOS-or-compute-only) / translation-layer / not now** — with reasoning.

---

## 1. Current renderer architecture — map it from the tree (cite file:line)
GPT must survey and document these before proposing anything:
- **GL abstraction layer** (`indra/llrender/`): `LLRender`/`gGL` (the immediate-ish wrapper + matrix stacks), `LLGLSLShader` + the reserved-uniform table (`llshadermgr.{h,cpp}` — `EShaderUniforms`, `mReservedUniforms`, the index-bound sampler system this fork depends on), `LLRenderTarget` (FBO wrapper), `LLVertexBuffer`, `LLImageGL`/`LLTexture`, `LLGLState`/`LLGLSLShader` state management, `LLVertexBuffer` VAO/VBO usage. This layer is the natural seam for a backend abstraction.
- **The pipeline** (`indra/newview/pipeline.cpp`): the deferred + HDR pipeline, shadow maps (sun cascades + the 2 spot/projector slots), reflection + hero probes, and the fork's BD-merge additions (froxel volumetric media, projector volumetrics, weather rain/lightning, the temporal passes, bloom pyramid, tonemap/CAS/AA). Document the render-target graph and pass ordering (`renderFinalize`) — this becomes explicit Vulkan barriers/dependencies.
- **Shaders**: GLSL under `app_settings/shaders/class1|2|3/`, the `#define` permutation system, `deferredUtil.glsl`, `postDeferredNoTCV.glsl`, the reserved-uniform binding convention. Count the shaders and permutations.
- **GL patterns that fight Vulkan**: any remaining immediate-mode / fixed-function, global `glEnable`/state, `glReadPixels` (used by e.g. the weather occlusion probe), occlusion queries, mid-frame state toggles (`setColorMask`, blend), live shader hot-reload (the fork reloads shaders on setting changes — hard under Vulkan PSOs).
- **Windowing / context** (`indra/llwindow/`): GL context creation, the 10-bit SDR swapchain (`RenderGLContext10bitSDR`), vsync, present.
- **The ReShade bridge** (`indra/newview/llreshadebridge.*`, `reshade-addon/`): publishes GL texture handles + matrices over a C ABI to a ReShade add-on. **This is GL-native and would break entirely under Vulkan** — ReShade *does* support Vulkan, but the bridge's handle/ABI model would need a full Vulkan redesign. Call this out as a first-class casualty.

## 2. Migration strategy options — analyze each (the core of the paper)
For each: effort, control, perf, risk, macOS story, and how it handles the fork's custom passes + shaders.
- **(A) RHI / backend abstraction** — introduce a render-hardware-interface that both a GL backend and a Vulkan backend implement; refactor `LLRender`/`LLRenderTarget`/`LLGLSLShader`/`LLVertexBuffer` to sit on it; port passes incrementally behind a runtime/​compile switch. The "correct," maximal-control, maximal-effort path. Discuss how LL's own recent renderer refactors (the PBR/deferred rewrite) did or didn't lay groundwork.
- **(B) GL-on-Vulkan translation** — run the existing GL renderer unchanged on **Zink** (Mesa's GL-over-Vulkan), **ANGLE** (GL ES over Vulkan/D3D — note ES vs desktop-GL gap), or a wrapper. Lowest code effort; gets Vulkan drivers underneath without a rewrite. Research Zink's maturity for a large desktop-GL 4.x app, its perf overhead, and whether SL's GL feature use is covered.
- **(C) macOS-only via MoltenVK** — target only the platform where GL is actually dying; keep desktop GL elsewhere. Scopes the effort to the real forcing function.
- **(D) Incremental compute offload** — keep GL for raster; add a Vulkan (or GL-compute) path for specific heavy passes (froxel, denoisers). Marginal.
- **Shader strategy** across options: GLSL → **SPIR-V** via glslang/shaderc, the permutation explosion, **SPIRV-Reflect** for descriptor layouts, and how live hot-reload survives (pipeline cache + async recompile). Or cross-compile GLSL and keep authoring in GLSL.
Give a **recommended option + phasing** with justification.

## 3. Hard Vulkan-specific problems (map each SL concept → Vulkan)
- **Memory**: adopt **VMA** (Vulkan Memory Allocator); map `LLImageGL`/VBO lifetimes; the fork's BD-merge RAM/VRAM pools (`bdmergetexpool`/`bdmergemeshpool`) interplay.
- **Pipeline State Objects**: GL's mutable state → immutable PSOs → combinatorial blowup across the shader permutations + blend/depth/colormask states this pipeline toggles mid-frame. Use `VK_EXT_extended_dynamic_state` + `VK_KHR_dynamic_rendering` to cut PSO count and drop render-pass objects. Pipeline caching to disk.
- **Descriptors / bindless**: the reserved-uniform + index-bound sampler system → descriptor set layouts; consider descriptor indexing / bindless for the texture-heavy avatar/material path.
- **Command buffers + threading**: record passes on worker threads; how SL's single-threaded render loop restructures.
- **Synchronization**: the RT graph (§1) → explicit `VkImageMemoryBarrier`/subpass deps / timeline semaphores; the temporal passes (froxel/projvol history ping-pong) need correct hazard tracking.
- **Present**: swapchain, the 10-bit format (`VK_FORMAT_A2B10G10R10`), vsync, HDR present.
- **Live shader reload**: the fork reloads shaders on setting toggles (weather/projvol/etc.) — design async PSO rebuild so this survives.

## 4. Fork-specific porting inventory
- Every BD-merge / weather / projvol / lightning GLSL pass + its `pipeline.cpp` GL dispatch → Vulkan equivalent (list them; estimate each).
- **ReShade bridge** → Vulkan redesign or drop (§1). Major, and it's a headline fork feature.
- Reserved-uniform/sampler system → descriptors (§3).
- The 10-bit swapchain, occlusion probe `glReadPixels`, any query objects.

## 5. Prior art to research + cite
- **Linden Lab's** public position/roadmap on Vulkan/Metal (has LL announced, prototyped, or rejected it? recent renderer-modernization work?).
- **Firestorm / other TPVs** discussions and any experiments.
- **Zink** production-readiness for large desktop-GL apps; real perf numbers.
- **MoltenVK** for shipping apps; the Mac GL-deprecation timeline.
- Large GL→Vulkan migrations as case studies (emulators like Dolphin/RPCS3 with backend abstractions; engines that added a Vulkan RHI). Extract lessons on abstraction seams + incremental cutover.

## 6. Phasing, effort, risk, and verdict
- A realistic **phased plan** (e.g., Phase 0 RHI seam + keep GL green; Phase 1 Vulkan backend for a narrow subset behind a flag; Phase 2 shader/SPIR-V pipeline; … Phase N feature parity + ReShade-Vulkan) with entry/exit criteria per phase and a "stay-shippable-the-whole-time" constraint.
- **Effort** honest order-of-magnitude (engineer-months/years) and the smallest viable slice that delivers real value.
- **Risk register**: regression surface, driver bugs, the ReShade casualty, live-reload, the sheer size vs a small fork's capacity.
- **Verdict** for THIS fork: recommend full / macOS-only / translation-layer / defer — with the reasoning, and what would change the answer.

## Deliverable
A single research/architecture paper covering §0–§6: the current-renderer map (file:line), the option analysis, the Vulkan-concept mapping, the fork porting inventory, cited prior art, and the phased plan + honest verdict. Skeleton code only where it clarifies (an RHI interface, a one-pass Vulkan outline). No build, no applied code. Cite sources for all external claims.
