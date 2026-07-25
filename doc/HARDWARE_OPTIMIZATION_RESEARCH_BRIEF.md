# Research brief — optimizing a Second Life viewer for 9950X / RTX 5090 32GB / 192GB RAM

**Audience:** ChatGPT deep research
**Project:** `bwlupus-ctrl/Alchemy-Machinima` — a **privately built** fork of Alchemy Viewer /
Linden Lab Second Life viewer. C++ / **OpenGL 4.6**, Windows, MSVC (VS 2026), CMake + vcpkg.
**Workload:** machinima. Long sessions, high visual fidelity, heavy scenes, and — uniquely — up to
50 client-only avatar **clones** rendered simultaneously (full rigged avatars with attachments,
animation and physics).

**The core premise:** the SL viewer is architected for a ~4-12 GB VRAM, 8-16 GB RAM, 4-8 core
machine. This machine is **192 GB RAM / 32 GB VRAM / 16 cores (32 threads)**. Many of its defensive
assumptions — texture streaming, discard bias, aggressive LOD, small thread pools — are not merely
unnecessary here, they may be actively costing frame time. **We want to know which assumptions to
delete, not which sliders to nudge.**

Because the build is private and never distributed, `-march=native`, PGO, LTO and even
GPL-incompatible experiments are all on the table.

---

## 1. What is ALREADY done (do not re-recommend these)

System-RAM side is well developed:
- **Decoded-texture RAM pool** — decoded textures held in system RAM feeding VRAM, with tiered
  residency (`BDMergeTexPoolEnable/Fraction/FloorMB/MaxMB/RecencyWindow/RegionGraceTTL`), auto-sized
  to roughly 96 GB.
- **Decoded-mesh RAM pool** — unpacked LODs served from RAM (`BDMergeMeshPool*`).
- **Heap cap auto-sized** to ⅞ of installed RAM; texture cache raised to 32 GB.
- **Parallel texture-cache body I/O** (`BDMergeCacheIOThreads`, default 4) — this fixed a genuine
  pathology: cache reads went from **781 ms mean / 5 s p95** to **46 ms / 200 ms**.
- **Decode thread ceiling** raised (`BDMergeDecodeThreadCeiling`, 24).
- **Capture-mode pin** — holds discard bias during a take so textures never degrade mid-shot.
- Instrumentation exists: `BDMergeTexSpikeLog` emits per-stage latency histograms and pool hit rates.
  **Tracy** is also available in the tree.

**Notably absent: anything VRAM-side.** No VRAM budget override, no residency policy change, no
texture-resolution cap change. All the memory work so far is host-RAM.

---

## 2. Research questions

### Q1 — GPU / 32 GB VRAM: which streaming assumptions can be deleted? ⭐
The viewer streams textures and juggles **discard bias** because it assumes VRAM is scarce. With
32 GB:
- What is the actual VRAM ceiling the viewer will use today, and what caps it? (`LLViewerTexture`
  budget logic, `RenderMaxVRAMBudget`-style settings if any, the `RenderTextureVRAMDivisor` path,
  driver-reported budget via `GL_NVX_gpu_memory_info` / DXGI.)
- Can texture streaming be effectively **disabled** — everything decoded held resident at full
  resolution — and what breaks if so? What is the failure mode when a scene genuinely exceeds VRAM
  (graceful eviction vs stutter vs crash)?
- Is the viewer re-uploading textures it already has resident? Quantify how to detect this.
- **Bindless textures / sparse textures / persistent mapping** — is the GL 4.6 feature set on a 5090
  usable to reduce per-frame binding and upload overhead in this codebase?
- Avatar **impostors** exist to save GPU. At 50 clones on a 5090, is impostoring now a *net loss*
  (the impostor render + the visual cost) versus just drawing them? Note this fork already forces
  clones to never impostor.
- Does anything meaningful still run at reduced resolution for performance reasons that this GPU
  makes pointless?

### Q2 — CPU / 9950X (Zen 5, 16C/32T, dual CCD) ⭐
The SL render loop is **main-thread-bound**. That is the central CPU fact.

- **What is actually on the main thread that need not be?** Identify candidates in a viewer of this
  lineage: object update, spatial partition rebuilds, avatar `updateCharacter`/skinning, LLSD
  parsing, inventory processing, texture-decode dispatch, mesh header parsing. Which have been moved
  off-thread in modern viewers (LL, Firestorm, Alchemy) and which have not?
- **Dual-CCD topology.** The 9950X is 2×8 cores with **separate L3 per CCD**; cross-CCD latency is
  substantial. The viewer's thread pools have no affinity awareness. Is CCD-aware affinity worth it —
  e.g. keeping the render/main thread and its most chatty helpers on one CCD, and batch work
  (decode, cache I/O) on the other? What is the evidence for this helping in comparable
  latency-sensitive workloads, and what is the risk of pinning going wrong?
- **Thread pool sizing.** Given 32 threads, are the current values (decode ceiling 24, cache I/O 4)
  sensible, and how should they be reasoned about rather than guessed? Consider oversubscription
  against the main thread's need for a free core.
- **AVX-512 on Zen 5.** Zen 5 implements a full-width 512-bit datapath (unlike Zen 4's
  double-pumped 256-bit), so AVX-512 is genuinely fast here. Where would it pay in this codebase?
  Specifically **OpenJPEG 2.5.4 J2K decode** (the known hot spot — is there a SIMD-optimized J2K
  path, an alternate decoder, or vcpkg build flags worth using?), plus mesh decompression, and the
  viewer's own math (`llmath`, matrix palette / skinning).
- **Build-level wins available to a private build:** `-march=znver5` / `/arch:AVX512`,
  **profile-guided optimization**, LTO, and MSVC-vs-Clang-cl for this workload. What realistic gains
  do these deliver on a main-thread-bound C++ application, and what are the pitfalls (PGO training
  set representativeness, AVX-512 downclocking — does Zen 5 downclock like Intel did?).

### Q3 — The clone workload specifically
This fork's signature load is **20-50 client-only avatar clones**, each a full rigged avatar. A
static audit already found that clones:
- are forced to `mUpdatePeriod = 1` (never throttled, never impostored), so each pays a full
  `updateCharacter` every frame;
- run avatar **physics** that is **never LOD-culled** (its minimum pixel area is 0), including a
  walk over ~705 visual params per clone per frame when physics updates;
- each maintain their own motion controller and matrix palette.

Given the hardware, what is the right architecture here — accept the cost because the machine can
take it, or introduce clone-specific LOD tiers? What would a principled budget look like (e.g. "50
clones at 60 fps" as a design target) and which of the above dominates?

### Q4 — Measurement strategy (answer this even if the rest is uncertain)
We have **Tracy** and a working custom instrumentation pattern. What exactly should we capture to
make the *next* optimization round evidence-driven rather than speculative?
- Which Tracy zones/counters to add and where, for a main-thread-bound GL viewer.
- How to attribute frame time between main thread, render submission, driver, and GPU on Windows/GL
  (GL timer queries, `GL_ARB_timer_query`, NSight/PIX equivalents for OpenGL).
- How to detect the specific pathologies above: VRAM thrashing, redundant uploads, cross-CCD
  migration, main-thread stalls waiting on a pool.
- What a good "machinima take" capture protocol looks like (scene, clone count, duration) so results
  are comparable across changes.

---

## 3. Constraints and preferences
- **OpenGL 4.6 on Windows.** Not Vulkan/D3D — a backend rewrite is out of scope. (If a specific
  interop trick is worth it, note it, but do not build the plan on a port. The fork does already
  carry a semaphore/memory-object interop change for DX12/Vulkan→GL.)
- **Private build**, never distributed — licensing-restricted or non-portable optimizations are
  acceptable; say so explicitly where that unlocks something.
- **Fidelity over frame rate**, within reason: this is machinima, so 60 fps stable during a take
  matters more than 200 fps, and *consistency* (no hitches) matters more than average.
- Prefer changes that are **measurable and revertible** over broad rewrites. This project has been
  burned by large speculative changes; it now works in small, verifiable steps.
- Note the render loop and its assumptions are shared with upstream, so anything that diverges
  heavily from LL/Alchemy has a maintenance cost — flag that where relevant.

## 4. Deliverable
A prioritized plan, each item with: expected magnitude of win, confidence, implementation effort,
risk, and **how to measure whether it worked**. Rank by (win × confidence) / effort. Explicitly
separate:
- **(a) settings/config changes** — free, revertible;
- **(b) build-level changes** — PGO/LTO/arch flags;
- **(c) code changes** — with rough scope;
- **(d) things not worth doing** on this hardware, and why.

Please be explicit about what could not be determined without profiling data, and what you are
inferring from general knowledge of the SL viewer codebase versus verifying from public source.

## 5. Sources worth consulting
- Second Life viewer source: `github.com/secondlife/viewer` — especially `llviewertexture*`,
  `lltexturefetch`, `llthreadpool` / `llworkerthread`, `pipeline.cpp`, `llvoavatar.cpp`.
- Alchemy Viewer: `github.com/AlchemyViewer/Alchemy` (this fork's parent — already has its own
  threading and HDR/tonemap changes).
- Firestorm: `github.com/FirestormViewer/phoenix-firestorm` for comparative perf work.
- Linden Lab performance-related release notes and the viewer's performance floater/autotune logic.
- AMD Zen 5 optimization guidance (AVX-512 implementation, CCD topology, cache hierarchy) and AMD's
  own compiler/PGO recommendations.
- NVIDIA Blackwell / RTX 5090 OpenGL guidance; `GL_NVX_gpu_memory_info` for budget queries.
- OpenJPEG performance work and any SIMD-accelerated JPEG2000 decoders (including whether commercial
  or differently-licensed decoders are viable for a private build).
- Tracy profiler documentation for GPU/OpenGL zone capture.
