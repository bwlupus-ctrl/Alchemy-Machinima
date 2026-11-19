# VRAM Telemetry & Accounting — Historical Implementation Plan

> **Superseded (2026-08-30):** this telemetry-only plan predates the staged
> governor now implemented in the Machinima client. Use `doc/VRAM_GOVERNOR.md`
> for the design and `doc/VRAM_GOVERNOR_CLAUDE_VERIFICATION.md` for the
> adversarial source review. The historical constraints below are retained as
> background, not as current implementation requirements.

**Source of truth:** `doc/HARDWARE_OPTIMIZATION_FINDINGS.md` (item "You are flying blind on VRAM" + "Recommended order" step 1). Target box: 9950X / RTX 5090 32GB / 192GB.
**Goal:** make VRAM pressure *visible* during a take. Nothing here changes rendering behavior; it is instrumentation + accounting, default-gated to the existing telemetry switch. This is the plan's #1 item ("everything else is unverifiable without this").

## State check (already done since the July findings — do NOT redo)
- `LLRenderTarget::sBytesAllocated` is now actively incremented/decremented (`llrendertarget.cpp:169,179,334,399,456,488,513`). The counter EXISTS; the open work is *surfacing* it (task 2).
- `TextureLoadFullRes` capture-gap has been partly folded into the capture pin (`llviewertexture.cpp:3196-3200`). Treat the capture-gap as VERIFY-ONLY (task 4), not new code.
- VRAM budget now routes through `BDMergeMemoryBudget` (`llviewertexture.cpp:32,577`), `sFreeVRAMMegabytes` (`:112`), `RenderTextureVRAMDivisor` default 2 (`:589`).
- Telemetry sink already exists: `BDMergeTexSpike` (`indra/newview/bdmergetexspike.{cpp,h}`), dump at `bdmergetexspike.cpp:151`, gate `BDMergeTexSpikeLog` (`:161`), interval `BDMergeTexSpikeLogInterval` 300s (`:162`).

## Scope (VRAM only — NOT SPGO, NOT the P1–P3 threading fixes, NOT the settings-only A/Bs)

### Task 1 — NVX VRAM/eviction poll (THE core item)
The `..._NVX` queries are defined but read nowhere (`indra/llrender/llglheaders.h:70-74`): `DEDICATED_VIDMEM_NVX 0x9047`, `CURRENT_AVAILABLE_VIDMEM_NVX 0x9049`, `EVICTION_COUNT_NVX 0x904A`, `EVICTED_MEMORY_NVX 0x904B`.
- Add a **1-second** poll (independent of the 300s dump interval) — `glGetIntegerv` each of the four NVX enums into KB values.
- **Guard once, on NVIDIA only:** cache availability by checking the GL vendor / extension `GL_NVX_gpu_memory_info` at first call (LL already tracks vendor in `LLGLManager`); no-op with zero cost on AMD/Intel. Never call `glGetError` in the hot path.
- Emit into the existing `BDMergeTexSpike` dump (or a sibling `LL_INFOS("BDMergeVRAM")` line): current-available MB, dedicated MB, and — the load-bearing signal — **`EVICTION_COUNT` delta since the previous poll** (a non-zero delta = the driver started paging VRAM→host over PCIe this window, the take-ruining hitch source). Also report `EVICTED_MEMORY` total.
- Home: a new `BDMergeTexSpike::pollVRAM()` (static, gated on `BDMergeTexSpikeLog`), called from an existing once-per-frame hook that already runs (e.g. `LLAppViewer::idle()` / the same place the tex-spike interval is checked). Track previous `EVICTION_COUNT` in a static so the delta survives across polls.

### Task 2 — Fold render targets into the reported VRAM "used"
The budget's `used` figure historically counted only textures + vertex buffers; render targets (~1.2 GB in this config) were excluded. `LLRenderTarget::sBytesAllocated` (`llrendertarget.cpp:39`, `.h:66`) already tracks them.
- Verify whether `BDMergeMemoryBudget` / the used-figure computation includes `sBytesAllocated`. If NOT, add it to the reported total (accounting/telemetry only — do not change the *budget ceiling* or eviction behavior).
- Surface `sBytesAllocated` (MB) as its own field in the VRAM dump so RT growth is separable from texture growth.

### Task 3 — GL_OUT_OF_MEMORY visibility (release)
`stop_glerror()` is gated on `gDebugGL`, so `createGLTexture` returns success even on OOM (`llgl.cpp` — verify current line). On NVIDIA/WDDM the alloc silently migrates to host; no crash, no log.
- Add a **sampled** (not per-texture) `glGetError()==GL_OUT_OF_MEMORY` check — e.g. once per VRAM poll tick, or immediately after a large `glTexImage2D`/RT allocation — counted into the telemetry. `glGetError` is a sync point: gate it on `BDMergeTexSpikeLog` and do NOT add it to per-draw/per-texture paths.

### Task 4 — VERIFY-ONLY: capture-gap
Confirm the screen-space downrez (`LLViewerLODTexture::processTextureStats` → `scaleDown`, `llviewertexture.cpp` ~3115) is now held during a capture via the pin (`:3196-3200`), and that `TextureLoadFullRes=1` still works standalone. Report the finding; only add code if a gap remains.

## Gating & safety (hard requirements)
- Everything gates on `BDMergeTexSpikeLog` (default on) — when off, byte-identical zero cost.
- NVIDIA-only for the NVX poll; complete no-op on other vendors.
- No new per-frame or per-texture `glGetError`/GL query in any hot path — 1 Hz poll only.
- Accounting/telemetry only: do NOT change the VRAM budget ceiling, discard bias, eviction, or streaming behavior. This item's job is to *see*, not to *tune*.

## Verification
1. Build `alchemy-bin` Release clean.
2. Run a 50-clone scene, push the camera through heavy geometry until it hitches; confirm the dump shows `CURRENT_AVAILABLE_VIDMEM` dropping and `EVICTION_COUNT` delta going non-zero at the hitch.
3. Confirm `sBytesAllocated` (RT MB) appears and tracks with post-process/shadow buffer allocation.
4. Confirm AMD/Intel path is a no-op (guard short-circuits) and `BDMergeTexSpikeLog=0` produces zero VRAM log lines.

## Out of scope (separate efforts, per the findings' recommended order)
- **SPGO** (build-level, the single biggest expected win) — its own plan.
- **P1 `sleepy_robin` busy-poll**, P2 decode-pool sizing, P3 thread priority/affinity — CPU, not VRAM.
- **Settings-only A/Bs** the user runs by hand: decode ceiling 24→12, `RenderHeroProbeUpdateRate` 1→6, shadow cascades 8192→4096, `TextureLoadFullRes=1`. No code.
- Do not lower the VRAM budget (explicitly "not worth doing").
