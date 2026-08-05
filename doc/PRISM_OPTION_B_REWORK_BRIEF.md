# Prism Option B — REWORK brief (must-fix): size-keyed exact-size scratch pool

An adversarial re-review **confirmed a rendering-corruption regression** in the current Option B
implementation. This brief replaces the broken sub-rect design with a correct one that still
saves VRAM. **Read `GEMINI.md` and `doc/AI_AGENT_CODEBASE_ORIENTATION.md` first.** Do NOT build
(Claude builds). Deliver compile-clean, `/WX`-clean source. Baseline: current working tree
(Option B/C + prior fixes). **Do not touch Option C** (optics: `prismLensF.glsl`, the floater
optics UI/commit/persistence) — those are confirmed correct. This brief only fixes the shared
scratch G-buffer.

## The confirmed bug (why the current design is wrong)
Current Option B allocates ONE shared scratch pack fixed at **1024×1024 square**
(`mPrismLensSharedScratchRT`, `pipeline.cpp` `allocatePrismLensBuffer`) and renders each capture
into a **logical sub-rect `[0,0,w,h]`** of it (`setPrismLensLogicalExtent` +
`applyPrismLensLogicalViewport`). But the **shared deferred fullscreen shaders sample the G-buffer
with normalized `[0,1]` UVs** — `vary_fragcoord = pos.xy*0.5+0.5` in
`class2/deferred/softenLightV.glsl`, consumed by `class1/deferred/deferredUtil.glsl` and
`class3/deferred/softenLightF.glsl`. A normalized `[0,1]` coordinate addresses the **full physical
1024²**, NOT the written `[0,0,w,h]` sub-rect. So for any capture below 1024² (nearly all — buckets
are 64/128/256/512/1024), the lighting pass samples a shrunk corner-crop plus **stale G-buffer from
the previous, larger sibling capture** in the unwritten region. Result: mis-scaled, bleeding
garbage. Confirm this for yourself by reading those three shaders before you start.

A fixed square pack cannot be made correct by rendering the full 1024² either, because captures are
**aspect-shaped** (`mTargetWidth`/`mTargetHeight` are bucketed independently) — a square render
stretches every non-square face. The only correct options are (a) exact-size aspect-matched targets
so physical == render, or (b) scaling the sample coord inside shaders shared with the main view
(rejected — too risky). This brief does (a), pooled so it still saves VRAM.

## The fix: a bounded, size-keyed pool of exact-size scratch packs
Replace the single fixed-1024² pack with a small pool (cap = `LLPrismLens::MAX_CAPTURES`) of
**exact-size, aspect-matched** scratch packs, reused across frames by their `(width,height)` key.
Because there is exactly **one auxiliary render per frame** round-robining across ≤ `MAX_CAPTURES`
captures, keying by size means: captures that share a size share one pack (VRAM win vs. the old
per-slot 3-pack); each pack persists across frames (no realloc churn); and every pack is exact-size
so the deferred normalized-UV sampling is correct and there is no cross-capture bleed.

### pipeline.h (around 965–974) — replace the single-pack members
Remove `mPrismLensSharedScratchRT`, `mPrismLensSharedScratchWaterDis`,
`mPrismLensSharedScratchWaterExclusionMask`, `mPrismLensLogicalWidth`, `mPrismLensLogicalHeight`.
Add a pool entry type and array + an active pointer, e.g.:
```cpp
// Option B (reworked): bounded pool of EXACT-SIZE auxiliary deferred scratch packs,
// reused across frames keyed by (width,height). Exact-size => physical == render, so the
// shared deferred fullscreen shaders' normalized-UV G-buffer sampling is correct and no
// sub-rect/logical-extent trick is needed. Captures sharing a size share one pack (VRAM win);
// packs persist across frames (no realloc churn). Retained outputs stay per-slot.
struct PrismLensScratch
{
    U32              width  = 0;
    U32              height = 0;
    U32              lastUsedFrame = 0;      // LRU key (gFrameCount)
    RenderTargetPack rt;
    LLRenderTarget   waterDis;
    LLRenderTarget   waterExclusionMask;
};
PrismLensScratch  mPrismLensScratchPool[LLPrismLens::MAX_CAPTURES];
PrismLensScratch* mActivePrismLensScratch = nullptr;   // set only during a prism aux render
LLRenderTarget    mPrismLensOutput[LLPrismLens::MAX_CAPTURES];   // unchanged
```

### pipeline.h (136–150) — method decls
Delete `setPrismLensLogicalExtent`, `clearPrismLensLogicalExtent`, `getPrismLensLogicalExtent`,
`applyPrismLensLogicalViewport`. Keep `bindPrismLensTarget` (simplified) and the
`allocate/release` methods (reworked signatures below). Add nothing new to the public surface you
do not need.

### pipeline.cpp — allocate → acquire
Rework `allocatePrismLensBuffer` into an acquire-by-size that selects or allocates a pool entry and
sets `mActivePrismLensScratch`. Suggested signature `bool acquirePrismLensScratch(U32 width, U32
height)` (or keep the name; drop the now-meaningless `slot` param). Algorithm:
1. Reject `width==0||height==0||width>1024||height>1024`.
2. If an entry already matches `(width,height)` **and** its `rt.deferredScreen/screen`
   (+ `deferredLight` when `RenderDeferredSSAO||RenderShadowDetail>0`) + `waterDis` +
   `waterExclusionMask` are `isComplete()` → set `mActivePrismLensScratch`, stamp
   `lastUsedFrame = gFrameCount`, return true. **No realloc.**
3. Else pick a target entry: first an empty/incomplete one; if all are live, evict the entry with
   the smallest `lastUsedFrame` (LRU). Release it, then allocate it **at exactly `(width,height)`**
   — mirror the current allocate exactly (deferredScreen `GL_SRGB8_ALPHA8` + `addDeferredAttachments`;
   `screen` `GL_RGBA16F`; `shareDepthBuffer(screen)`; `deferredLight` `GL_RGBA16F` only when
   `needs_deferred_light`; `waterDis` `GL_RGBA16F`; `waterExclusionMask` `GL_R8`) — but with the
   passed `width,height`, NOT 1024. On any failure release the entry, clear
   `mActivePrismLensScratch` if it pointed here, return false.
4. Set `width/height/lastUsedFrame`, `mActivePrismLensScratch = &entry`, return true.

`allocatePrismLensOutput` is unchanged (outputs stay fixed 1024²).

Add a small helper the render path uses: `RenderTargetPack* getActivePrismLensScratchPack() {
return mActivePrismLensScratch ? &mActivePrismLensScratch->rt : nullptr; }`.

Delete `setPrismLensLogicalExtent`/`clearPrismLensLogicalExtent`/`getPrismLensLogicalExtent`/
`applyPrismLensLogicalViewport` entirely. `bindPrismLensTarget(LLRenderTarget& target)` becomes just
`target.bindTarget();` (exact-size packs already install a full-size viewport via `bindTarget`; no
sub-rect override remains).

`getWaterDisTarget()` / `getWaterExclusionMaskTarget()`: return
`mActivePrismLensScratch->waterDis` / `->waterExclusionMask` when
`sPrismLensRender && mActivePrismLensScratch && mRT == &mActivePrismLensScratch->rt`, else the main
`mWaterDis` / `mWaterExclusionMask` (same gate intent as today, keyed off the active entry).

`releasePrismLensBuffer(slot)` / `releasePrismLensBuffers()`: release all pool entries' targets and
zero their `width/height`, and set `mActivePrismLensScratch = nullptr`. (Per-slot semantics are
gone; releasing frees the whole pool. Rename to a no-arg `releasePrismLensScratch()` if cleaner, and
update callers — grep `releasePrismLensBuffer`.)

### llprismlens.cpp (render path ~4462–4710)
- `render_width/height` stay `= output_width/height` (the exact bucketed size). Good — that is now
  also the physical scratch size.
- Replace `allocatePrismLensBuffer(slot, render_width, render_height)` (4476) with the acquire call
  `gPipeline.acquirePrismLensScratch(render_width, render_height)` (same deferRetry-on-false).
- Replace `gPipeline.mRT = &gPipeline.mPrismLensSharedScratchRT;` (4484) with
  `gPipeline.mRT = gPipeline.getActivePrismLensScratchPack();` and bail (deferRetry) if null.
- **Delete** the `setPrismLensLogicalExtent(...)` call + its failure branch (4485–4489).
- The manual viewport/scissor block (4664–4672) stays but now covers the FULL exact-size target
  (render == physical) — that is correct and no longer a sub-rect. (You may keep it as-is.)
- `LLPipeline::RenderTargetPack& rt = gPipeline.mPrismLensSharedScratchRT;` (4693) →
  `RenderTargetPack& rt = *gPipeline.getActivePrismLensScratchPack();` (already validated non-null).
- The copy (4704–4710) `copyContents(rt.screen, 0,0,render_w,render_h, 0,0,output_w,output_h, ...)`
  stays; render==output so it is a straight full-rect copy (`GL_NEAREST`). The per-slot output and
  its `mTextureRegionScale/Offset` publication are unchanged and already correct.

### Comments
Update the stale block at `pipeline.h` ~965 and `allocatePrismLensBuffer`'s comment at
`pipeline.cpp` ~1347 to describe the size-keyed exact-size pool (not "single exact-size pack shared
serially").

## Invariants — do not weaken
`setViewNoBroadcast` only (never `setView`); full `ScopedPrismRenderState` save/restore on every
exit path (the acquire/evict must not leave `mActivePrismLensScratch` or `mRT` dangling on any early
return — clear/restore them); one aux scene render per frame; composite samples linear-HDR before
tonemap; **Prism-OFF frame byte-identical**; default-off; don't touch clone / overlay /
`actorghostF` / `LLActorMover`; never `git add -A`; shaders compile at RUNTIME (this brief changes
no shader `#ifdef`s); warnings are errors — cast `LLSD::Real`→`F32`, cover all enum `switch` cases,
no unused params (drop `slot` if unused rather than leaving it).

## Deliver
Edited files + a short note of what changed at which file:line, and confirm: every scratch pack is
allocated at the capture's exact `(width,height)` (physical == render, no sub-rect); packs are
reused by size and persist across frames (no per-frame realloc); the pool is bounded to
`MAX_CAPTURES` with LRU eviction; two captures at different sizes never bleed (each has its own
exact-size pack); `mActivePrismLensScratch`/`mRT`/water getters are consistent and restored on every
exit; Option C untouched; `git diff --check` clean. Claude will adversarially re-review (focus:
exact-size correctness, no dangling active-pointer on early returns, no leak, Prism-off
byte-identical) and build.
