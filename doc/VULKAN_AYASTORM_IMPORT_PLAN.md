# Vulkan via AYAstorm — Import / Transplant Plan (recon-grounded)

Companion to `doc/VULKAN_MIGRATION_DEEP_RESEARCH.md` and the GPT feasibility verdict (AYAstorm = Vulkan *donor*; keep Alchemy-Machinima as product + GL oracle; two-branch spike). This doc records the **verified reconnaissance** of `mayatonton/phoenix-firestorm` and turns the strategy into concrete first steps. It is a PLAN — no code imported, nothing built.

## 0. Reconnaissance (verified 2026-07-30 via GitHub API, not built/run)
Repo `mayatonton/phoenix-firestorm`. Full Vulkan lineage of branches: `feature/ayastorm-r40-vulkan-migration` → `r41-vk-canonical` / `r41-gl-removal` → `r42-gl-removal` / `r42-macos-moltenvk-runtime` / `r42-macos-stabilization` / **`feature/ayastorm-r42-phase2`** (GPT's pick) + `ref/phase2-perdraw-ownership`, `dev/ayastorm-vk-3os`, `dev/ayastorm-vk-premt`.

**Vulkan substrate in `r42-phase2` (hand-written, bytes):**
| File | Size | Role |
|---|---:|---|
| `indra/llrender/llvkloader.cpp` | **536,732** | the monolithic VK backend (device/swapchain/VMA/cmd/pipeline/present/multi-window) |
| `indra/llrender/llvkloader.h` | 65,065 | its interface |
| `indra/llrender/llvkuboreg.cpp` / `.h` | 72,261 / 3,953 | UBO/SSBO registry |
| `indra/llrender/llvkcontract.cpp` / `.h` | 31,632 / 4,648 | validation/contract + perf instrumentation |
| `indra/newview/llvkbucket.cpp` / `.h` | 13,580 / — | persistent pipeline-sorted draw buckets |
| vendored: `volk.c/.h` (537 KB), `indra/llrender/vendor/vk_mem_alloc.h` (VMA), `indra/cmake/Vulkan.cmake`, `VulkanGltf.cmake` | — | third-party + build integration (drop-in) |

**Seam files (retain names; VK-backed underneath):** `llrendertarget.cpp` (32 KB), `llvertexbuffer.cpp` (43 KB), `llglslshader.cpp` (155 KB), `llrender.cpp` (61 KB). Our tree has the same classes with fork additions.

**ReShade bridge — ABSENT in `r42-phase2`** (`indra/newview/llreshadebridge.{cpp,h}` → HTTP 404). Confirmed casualty.

**License:** AYAstorm's new VK files are LGPL 2.1 (per GPT) — compatible with the viewer's LGPL lineage; preserve copyright/source notices + audit third-party (volk/VMA) licenses.

## 1. Strategy (endorsed, with the data behind it)
Recommended path = **transplant the AYAstorm VK substrate into Alchemy-Machinima; keep the Alchemy GL tree as the visual/behavioral oracle; run a two-branch base-selection spike before committing lineage.** The recon supports this: the substrate is large but concentrated (portable as a unit) and the seam classes match ours, but the ReShade bridge + our BD-merge passes + reserved-uniform/sampler system are not in AYAstorm and must be rebuilt — which is why we do NOT wholesale-switch to Firestorm.

**Reframe the ask:** this is not a "patch." It is a renderer transplant + a feature-parity project. Treat effort as multi-week-to-month, needing a Vulkan SDK (LunarG) + iterative builds.

## 2. First-import unit (Branch B: Alchemy + AYAstorm VK substrate → one frame)
Import as ONE coherent foundation (do NOT also rewrite it into a clean RHI yet — keep it traceable to source):
1. Build integration: `Vulkan.cmake`/`VulkanGltf.cmake`, volk, VMA, glslang/SPIR-V, `llvkloader/uboreg/contract`, `llvkbucket`.
2. Device/caps/validation/swapchain; frame serials, fences, deferred destruction, PresentEngine.
3. VK backing for `LLRenderTarget`, `LLImageGL`, `LLVertexBuffer` behind the retained class names.
4. GLSL→SPIR-V + SPIRV reflection → descriptor layouts + the UBO registry.
5. Pipeline create/cache + command-recording infra.
6. Contract + perf instrumentation.
Defer the bindless heap / mega-buffer / persistent-bucket / MDI performance layers to later gated stages — they are not prerequisites for the first correct frame.

## 3. Seam reconciliation (Alchemy fork additions vs AYAstorm VK versions)
Our `LLRenderTarget`/etc. carry fork-specific additions to reconcile against AYAstorm's VK-backed versions:
- `LLRenderTarget`: our stencil/depth-format selection, direct FBO access, external attachment names (weather occlusion map, projvol history, etc.).
- The **reserved-uniform + index-bound sampler system** (`llshadermgr` `EShaderUniforms`/`mReservedUniforms`) → Vulkan **descriptor sets**. Every fork feature that added a reserved sampler (weather occlusion map, projvol, lightning) rides this.
- Live shader hot-reload (settings toggles reload shaders today) → async PSO rebuild + pipeline cache.

## 4. Fork casualties to rebuild for Vulkan (the real cost)
- **ReShade bridge** — absent in AYAstorm; redesign for Vulkan resource identity + layout transitions + resize/generation semantics + ReShade's Vulkan event model. Publish the same contract (camera matrices, depth, G-buffer albedo/ORM/normal/emissive, HDR color, visible-diffuse + coverage sidecars, motion vectors, history/reset) as **backend-neutral semantics**, not the raw GL texture-name/format ABI.
- Every BD-merge / weather / projvol / lightning GLSL pass + its `pipeline.cpp` dispatch → VK pipeline + descriptors + barriers.
- Mirrors / hero probes / high-res capture / cloned entities / director systems — re-express as render-graph nodes with explicit resource semantics.

## 5. The base-selection spike (before committing lineage) — GPT's gates
Two ~10-day branches: **A** = Firestorm/AYAstorm base + port only the "machinima parity spine" (ReShade semantics, one mirror/hero path, snapshot, director camera, one clone scene); **B** = Alchemy base + import enough VK substrate to init Vulkan, compile+reflect our shader family, create the deferred targets, render one world frame (or replay a captured workload). Decide by evidence: builds on Windows/NVIDIA + Linux; zero unexplained validation errors; no device loss on resize/minimize/teleport/shader-reload; correct avatars/rigging/alpha/water/HUD/media; exact ReShade semantic parity; mirror/probe/snapshot parity; p50/p95/p99 frametime vs the Alchemy GL oracle; real CPU/GPU overlap; documented Firestorm-vs-Alchemy conflict count; credible upstream-maintenance story. Likely outcome: an **AYAstorm-derived VK renderer inside Alchemy-Machinima**.

## 6. Honest caveats (from AYAstorm's own docs, per GPT)
- Not yet faster than GL: a crowd+mirror RTX 5090 test was CPU-record→GPU→present serialized (24.4 ms of a 48.8 ms frame in multi-pass emission); parallelization moved ~38→48 FPS but it remains an active recovery project.
- Stability gaps: `VK_ERROR_DEVICE_LOST`, a shutdown crash in `LLVertexBuffer` teardown, swapchain-recreation churn, ~15.6–20 FPS in a macOS run — the merged PR added the *plan*, not all fixes.
- MoltenVK reaches the macOS login UI without a GL fallback (real, but not production-ready).

## 7. What this environment can/can't do
- **Can (done):** API recon, verify claims, plan. **Can:** author the detailed Branch-B import brief for Codex/GPT; scaffold the Alchemy Vulkan branch skeleton + CMake seam.
- **Can't in one shot:** the transplant itself — it needs the Vulkan SDK, a large cross-tree merge of a 537 KB monolithic loader, and many build/validate iterations. Cloning the AYAstorm branch is a large download (decide before pulling).

## 8. Recommended immediate next actions
1. Pin `feature/ayastorm-r42-phase2` at a known SHA (recon tip: `3f559081e736db7ad6ebd8d394e65110de716602`).
2. Decide: author a detailed **Branch-B import brief** (Codex/GPT executes the substrate import to first-frame), or scaffold the Alchemy `vulkan-spike` branch + CMake/VK-SDK seam here first.
3. Keep Alchemy `develop` (GL) as the untouched oracle; do the spike on a branch.
4. Rebuild the ReShade contract as backend-neutral semantics (do NOT Vulkanize the raw GL ABI).
