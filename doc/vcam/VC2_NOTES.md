# VC2 implementation notes

## Implemented

- Added `LLPrismPreviewService` with fixed, generation-bearing capture demand and owner-scoped page visibility APIs. Disabled, hidden, or demand-free frames return before scratch/GL work (`indra/newview/llprismpreview.h:79`, `indra/newview/llprismpreview.cpp:24`, `:102`).
- Extended capture runtime with preview intent, preview-only state, independent deadline/entitlement, and publication serial. Preparation reuses visible/Gate captures and gives only unshown demanded captures the fixed 192x108 bucket (`indra/newview/llprismlens.h:316`, `indra/newview/llprismlens.cpp:2533`, `:2661`, `:3168`).
- Added strict visible/Gate-first water filling, preview leftover allocation, separate preview round-robin/deadline handling, no catch-up, and one shared auxiliary-attempt token per presented frame (`indra/newview/llprismlens.cpp:3414`, `:3468`, `:3478`, `:3575`).
- Added the pinned `mPrismPreviewScratch` pack and allocate/bind/release APIs outside the eight-entry visible scratch LRU. Release paths clear the active pointer and all preview-owned attachments (`indra/newview/pipeline.h:176`, `:1368`, `indra/newview/pipeline.cpp:1830`, `:1877`, `:1962`).
- Added `ScopedPrismPreviewQuality`, nested inside the existing full render-state scope, to push/restore shadow detail, SSAO, directional/projector volumetrics, `RenderFarClip`, and `sPrismPreviewRender`; both camera projections cap preview far range at 64 m (`indra/newview/llprismpreview.cpp:155`, `:171`, `indra/newview/llprismlens.cpp:8176`, `:8261`, `:8319`).
- Shadow generation, SSAO/light-map/blur, directional volumetrics, projector volumetrics, and reflection-probe update all explicitly gate on `sPrismPreviewRender`. The normal probe update remains untouched in the `probe_before_present` pre-swap block (`indra/newview/llprismlens.cpp:8445`, `indra/newview/pipeline.cpp:18995`, `:20957`, `:25628`, `indra/newview/llreflectionmapmanager.cpp:265`).
- Every preview seeds the reused deferred-light/AO target to neutral white before suppressed SSAO dispatch. The pure sentinel regression shares the production neutral value (`indra/newview/pipeline.cpp:25618`, `indra/newview/llprismpreview.h:19`, `indra/newview/tests/llprismpreviewpolicy_test.cpp:78`).
- Added pure scheduler regressions for visible-first allocation, preview/capture rate ceilings, no-catch-up deadlines, and stale-AO neutralization; registered source/header/test wiring in CMake (`indra/newview/tests/llprismpreviewpolicy_test.cpp:23`, `:51`, `:67`, `:78`, `indra/newview/CMakeLists.txt:862`, `:2911`).

## Verification and deviations

- Per request, no configure, build, executable test, commit, fetch, or upstream operation was run. Static verification used symbol/anchor review, `git diff --check`, trailing-whitespace checks, and LF checks.
- Atlas composition and measured timestamp/token budget diagnostics remain VC3/VC4 work as assigned by section 8; preview scene attempts already remain subordinate to the existing global capture budget and adaptive controller.

## VC3 review follow-ups

- Surface-Lens preview culling now receives the same scoped far plane as its projection (`indra/newview/llprismlens.cpp:8591`).
- The stale-AO regression invokes the production `seedPreviewAO` decision, and the renderer uses that same function before clearing the reused attachment (`indra/newview/tests/llprismpreviewpolicy_test.cpp:80`, `indra/newview/pipeline.cpp:25744`).
- Preview-only logical extents preserve non-16:9 producer aspect inside the fixed pack (`indra/newview/llprismpreview.h:55`, `indra/newview/llprismlens.cpp:2708`, `:3266`).
- Pinned-pack allocation now occurs inside `ScopedPrismRenderState` and retries back off for one second (`indra/newview/llprismpreview.cpp:181`, `indra/newview/llprismlens.cpp:8413`).
- Null demand is rejected, visibility-owner overflow asserts, sub-epsilon entitlement is discarded, and preview-quality publications are excluded from both world composite builders (`indra/newview/llprismpreview.cpp:28`, `:93`, `indra/newview/llprismpreview.h:19`, `indra/newview/llprismlens.cpp:8968`, `:9235`).
