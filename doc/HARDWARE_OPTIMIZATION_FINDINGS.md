# Hardware optimization findings — 9950X / RTX 5090 32GB / 192GB RAM

Deep research pass, 2026-07-25, read against `develop` @ `a6744226a61` **and the live
`%APPDATA%\AlchemyMachinima\user_settings\settings.xml`**. Nothing here was profiled — this is
static analysis plus config reading. Every `file:line` claim was verified in-tree; inferences are
marked.

**Overall conclusion: the premise "the viewer assumes a small machine, delete its defenses" was
largely already acted on.** VRAM budget is wide open, AVX2 and LTO are on, impostoring and
complexity-mute are disabled. The remaining wins are (1) one build-level change nobody has tried,
(2) two threading pathologies, and (3) one texture path the capture pin does not cover.

---

## Corrections to the premise (verified)

| Assumed open | Actual state |
|---|---|
| VRAM budget capped low | **Already ~25.6 GB.** `RenderTextureVRAMDivisor` is 1 upstream (`settings.xml:13410`); budget math at `llviewertexture.cpp:500-507`. NVX detection is correct at 32 GB (`llgl.cpp:1350-1358`), no overflow. Nothing to delete. |
| Impostors still costing | **Already off** — `BDMergeMachinimaHighLOD=1` (`llvoavatar.cpp:11584, 11596, 4564`), complexity auto-mute off (`:9149-9158`). |
| `/arch:AVX2` available | **Already ON** (`USE_AVX2:BOOL=ON`, `00-Common.cmake:213-217`). |
| LTO available | **Already ON** (`USE_LTO:BOOL=ON`; `/GL /LTCG` confirmed in `alchemy-bin.vcxproj:51`). |
| Anisotropic filtering | **Already 16** (stock is 0/off). |
| PGO | **Not present** — and every prerequisite is already installed. See B1. |
| vcpkg dependencies | **Built at SSE2 baseline** — `VCPKG_C_FLAGS_RELEASE ""` in
`indra/vcpkg/triplets/x64-windows-alchemy.cmake`. The viewer is AVX2; its libraries are not. |

---

## THE TOP ITEM — Sample PGO (SPGO)

**Every prerequisite is already on this machine, verified:** MSVC `14.51.36231` (the `/spgo`
minimum), `sptaggregate.exe` / `spdconvert.exe` / `sptdump.exe` / `spddump.exe` in
`bin/Hostx64/x64/`, `xperf.exe` in the Windows Performance Toolkit, `/GL` already on via LTO, and
`/DEBUG:FULL` already on (`00-Common.cmake:137-146`). **The LBR high-fidelity path requires AMD
Zen 4+ — the 9950X qualifies.**

Why it fits *this* application unusually well:
- SPGO profiles the **release binary** at negligible overhead, so the training workload can be an
  actual machinima take. Classic instrumented PGO would make the viewer too slow to drive
  interactively — and would make the profile unrepresentative.
- Main-thread-bound C++ with deep call graphs and heavy virtual dispatch is the *best case* for
  profile-guided inlining, hot/cold splitting and **speculative devirtualization** — which is
  exactly what the viewer's frame loop is.

Expected **5-15%** on the main thread (Microsoft's documented range). Effort ~half a day. Revert =
drop `/spdin`. Add `/spgo` near `00-Common.cmake:137`, capture with
`xperf -on LOADER+PROC_THREAD+PMC_PROFILE -LastBranch PmcInterrupt`, then `sptaggregate` →
`spdconvert /mode:LBR` → rebuild with `/spdin`.

**Pitfall:** profile a *take*, not startup or login. The linker reports a coverage percentage —
reprofile when it falls below ~90%.

---

## Threading pathologies (verified in source)

### P1 — Every idle pool thread busy-polls at 1 ms
`indra/llcommon/threadpool.cpp:36-57` installs `sleepy_robin` on every pool thread (`:141`):
`suspend_until()` is `Sleep(1)` and **`notify()` is deliberately a no-op**. `LLThreadSafeQueue`
waits on a *boost fiber* condvar (`llthreadsafequeue.h:187-188`), which yields to that scheduler
rather than blocking the OS thread. Combined with `timeBeginPeriod(1)`
(`llwindowwin32.cpp:4988`), that is **~33 pool threads × ~1000 wakeups/sec ≈ 33k context switches
per second while completely idle** — i.e. during a steady-state take. `doFrame` can pause the
texture cache/fetch threads (`llappviewer.cpp:1651-1656`) but **cannot pause the ThreadPool-based
decode pool**, so all 24 poll forever.

Second consequence: because `notify()` does nothing, **newly posted work waits up to 1 ms before any
worker notices** — a silent tax on the CacheIO win already landed.

Fix: replace with an event + real `notify()`, backoff 1 → 16 ms when idle. ~30-40 lines, gate it.

### P2 — Decode pool is sized against logical, not physical, cores
`llappviewer.cpp:2271`: `llclamp(cores - 6, 2, ceiling)` with `hardware_concurrency() = 32` and
`BDMergeDecodeThreadCeiling = 24`. But 32 is **logical** threads on **16 physical cores**. 24
CPU-saturating J2K decoders occupy 12+ physical cores; add main, render-adjacent, fetch, cache and 4
CacheIO and you are at or past 16 physical cores whenever the decode queue is deep — scene load,
teleport, region crossing, clone rez. The main thread then shares a physical core with a decoder and
loses ~20-35% throughput to SMT contention (**inferred magnitude**).

**This is the most likely mid-take hitch mechanism, and it is a free A/B: set the ceiling to 12.**

Correct principle, which the code does not encode: **CPU-bound pools size against physical cores
minus a reserve; IO-blocking pools may oversubscribe freely.** CacheIO at 4 is fine.

### P3 — No thread affinity or priority anywhere
The only affinity code in the tree is dead (`llwindowwin32.cpp:583-613`, `#if 0`).
Recommendation is the **soft** version first: `SetThreadIdealProcessor` for main +
`THREAD_PRIORITY_BELOW_NORMAL` for decode workers — most of the benefit, none of the throughput
cliff of hard masks. Derive CCD grouping from `GetLogicalProcessorInformationEx(RelationCache)`
grouped by L3 mask; do **not** hardcode CPU 0-15 = CCD0. Never pin main to CPU 0 (it takes DPC/
interrupt load) — the dead code did exactly that.

---

## The texture path the capture pin does NOT cover

`BDMergeCaptureModePin` pins the *memory-pressure* discard bias — but the **screen-space** downrez is
independent of it and still runs every frame:
`LLViewerLODTexture::processTextureStats()` (`llviewertexture.cpp:3115-3165`) derives discard from
on-screen pixel area and calls `scaleDown()` regardless of the 25.6 GB budget. Push the camera in and
the texture must be **re-fetched, re-decoded and re-uploaded** — a hitch the pin does not close.
Avatar *baked* textures are exempt (`BOOST_AVATAR_BAKED` guard); mesh attachments and scene textures
are not.

**The switch already exists: `TextureLoadFullRes = 1`** (`settings.xml:17455`), which forces
`mDesiredDiscardLevel = 0` (`llviewertexture.cpp:1774`, `:3092`), bypasses `scaleDown()`, and
collapses progressive fetch into a single full-size request (`lltexturefetch.cpp:2572-2579`) instead
of 5→3→1→0, each step a full `glTexImage2D` + `glGenerateMipmap` of the whole chain
(`llimagegl.cpp:1824-1878`, `:955`). `glTexStorage2D` is never used anywhere in the tree.

---

## You are flying blind on VRAM (do this first)

`GL_OUT_OF_MEMORY` is never checked in release — `stop_glerror()` is gated on `gDebugGL`
(`llgl.cpp:2509-2516`), so `createGLTexture` returns success regardless. **Inferred:** on
NVIDIA/WDDM2 the driver will not fail the allocation; it silently migrates resources to host memory
over PCIe. No crash, no log line — just non-deterministic frame times. That is exactly the
take-ruining hitch profile, and there is currently **zero** visibility into it.

`GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX`, **`..._EVICTION_COUNT_NVX`** and
`..._EVICTED_MEMORY_NVX` are **defined at `llglheaders.h:70-74` and queried nowhere.** ~30 lines to
poll once/second into the existing `BDMergeTexSpike` dump. `EVICTION_COUNT` incrementing is the
unambiguous signal that paging started.

Also: the VRAM budget's `used` figure (`llviewertexture.cpp:494`) counts only textures and vertex
buffers — **render targets are excluded**. `LLRenderTarget::sBytesAllocated` exists
(`llrendertarget.cpp:39`) and is never fed in; ~1.2 GB unaccounted in the current config.

---

## Live config items worth revisiting

| Item | Live value | Note |
|---|---|---|
| `RenderHeroProbeUpdateRate` | **1** | All **6 cube faces re-rendered every frame at 2048²** ≈ 25 Mpx/frame of extra scene rendering. Possibly the single largest GPU cost — *if* a hero object is within `RenderHeroProbeDistance` (16 m). |
| `RenderHeroProbeConservativeUpdateMultiplier` | 4 | **Dead setting** — read into a static (`pipeline.cpp:338, 1522`), consumed nowhere. Throttles nothing. |
| Shadow cascades | 8192/8192/6144/4096 | ≈754 MB plus fill × 4 passes × 50 avatars. 8192² exceeds what a 4K frame can resolve for cascade 0. |
| Spot shadows | 4096 × 6 | ≈402 MB more. |
| `RenderReflectionProbeResolution` | 128 | Cheap fidelity win at 256 (VRAM 134 MB → ~537 MB, irrelevant here). |
| `USE_TRACY_GPU` | **OFF** | Without it you cannot distinguish "main thread slow" from "blocked in SwapBuffers". `LL_PROFILE_GPU_ZONE("doFrame")` already exists at `llappviewer.cpp:1374`. |

---

## AVX-512: verified NO

Zen 5 has a true full-width 512-bit datapath and does **not** downclock like Intel (worst case ~10%,
5.7 → 5.3 GHz), so the frequency objection is dead. **The codebase objection is fatal instead:**
`LLQuad` is `typedef __m128` (`llsimdtypes.h:34`), `LLVector4a::mQ` is `LLQuad`
(`llvector4a.h:376`), `LLMatrix4a` is 4× `LLVector4a` (`llmatrix4a.h:38`). These are **fixed
128-bit** types — AVX-512 cannot widen a 4-float vector without an SoA rewrite of the whole math
library. And J2K's dominant cost is T1 EBCOT arithmetic decoding, which is **bit-serial** — SIMD
width is irrelevant to it (the DWT does vectorize, and is the minority).

## OpenJPEG: threading compiled in but unused
`indra/llimagej2coj/llimagej2coj.cpp` **never calls `opj_codec_set_threads`**, yet the shipped
`openjp2.lib` contains `opj_codec_set_threads`, `opj_has_thread_support`,
`opj_t1_clbl_decode_processor`, `opj_dwt97_decode_v_job_t` etc. **Inferred:** `OPJ_NUM_THREADS` env
var would enable it. **Not recommended naively** — the viewer creates a fresh codec per decode
(`llimagej2coj.cpp:404`), so each decode would build and tear down a pool; 24 workers × N sub-threads
is absurd. A latency-only experiment for single huge textures.

**Kakadu (`kdu`) is already wired** in `indra/vcpkg.json` / `indra/cmake/LLKDU.cmake`, gated on
`INSTALL_PROPRIETARY`. Substantially faster than OpenJPEG. Since this build is never distributed the
only barrier is a licence — the one place "private build" genuinely unlocks something large.

## Other verified structural findings
- **`LLPipeline::updateGeom`'s `max_dtime` parameter is never used** (`pipeline.cpp:3423`) — the
  whole `mBuildQ1` drains every frame, unbounded, as does `processPartitionQ`. Clearest structural
  hitch source in the frame. Instrument `mBuildQ1.size()` as a Tracy plot before changing anything.
- **`LLVOAvatar::updateImpostors()`** (`llvoavatar.cpp:11563-11580`) regenerates every invalid
  impostor **in one frame with no cap**, and invalidation is camera-angle driven (`:3184`) — so a
  camera pan invalidates all of them simultaneously. Mute-listed avatars are still on this path even
  with `BDMergeMachinimaHighLOD`.
- **`LLRiggedVolume::update`** (`llvovolume.cpp:5042-5200`) is per-vertex, per-face, serial, with no
  cross-face dependencies — embarrassingly parallel, single-threaded only by history. Gate on
  measurement.
- **Impostor cost is three geometry submissions plus two forced `gGL.flush()` syncs**
  (`pipeline.cpp:16302-16487`) versus one main-pass draw. **Inferred:** net loss under ~100 px on a
  5090.

---

## Recommended order

1. **VRAM + queue telemetry** (NVX eviction counters; `mBuildQ1` and decode-queue depth as Tracy
   plots; turn on `USE_TRACY_GPU`). *Everything else is unverifiable without this.*
2. **Baseline capture** — fixed 3-minute take, 50 clones, recorded flycam path so runs are
   identical, after a warm-up pass. Report **p50/p95/p99/max**, never average.
3. **Free A/Bs, one at a time:** decode ceiling 24 → 12; `RenderHeroProbeUpdateRate` 1 → 6; shadow
   cascades 8192 → 4096; then `TextureLoadFullRes = 1`.
4. **SPGO** (the single largest expected win).
5. **`sleepy_robin` fix**, then soft thread priority/affinity.
6. Only then consider parallelizing rigged-volume update or geometry fill — the "large speculative
   change" category this project has repeatedly been burned by.

## Explicitly not worth doing
Global `/arch:AVX512` · clang-cl migration · bindless textures (the indexed-texture batching at
`llviewershadermgr.cpp:574-589` already collapses most binds) · sparse textures · multi-draw-indirect
(blocked by per-draw model matrix and per-mesh skinning palette) · persistent-mapped buffers ·
re-enabling `ENABLE_GL_WORK_QUEUE` · lowering the VRAM budget.
