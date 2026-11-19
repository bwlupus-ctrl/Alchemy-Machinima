# Machinima VRAM Governor — Claude Adversarial Review Response

**Target:** uncommitted governor + pressure-aware capture ownership in `I:\alchemy-machinima`.
**Build:** `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`, SHA-256 `9049E04D…C90866` — **verified to match the file on disk.**
**Method:** three passes — capture↔governor + state-machine re-derivation (deep reasoning), accounting-parity + UI/layout + settings (breadth), and hand-verification of the capture crux, the P0/P1 logic, and the final bias pipeline. Read-only; no build/run/edit.

## 1. Verdict: READY WITH RISKS

The reported bug is fixed and verified: capture mode no longer defeats the VRAM cap. The two-tier capture pin, both full-resolution request sites, and `TextureLoadFullRes` all yield to the latched VRAM pressure state and resume only after hysteretic recovery to Normal. The accounting is sound, the UI is correct and does not overlap the tone-mapping controls, and a real Release build linked and matches its SHA. **No P0/P1 regressions were found by any pass.** This port also closes the two findings that were still open against the sibling Firestorm tree (the recovery-hold bias ramp and the frame-0 bias initializer).

The "risks" are two P2 behaviors in which the governor keeps capture quality suppressed for reasons texture discard cannot fix — conservative-but-correct on their face, but for a capture tool they can mean "quality never resumes this session." They are policy decisions, not defects. Plus one small pre-existing accounting undercount unrelated to this change.

## 2. Findings

### P2-1 — Driver-headroom recovery dead zone can suppress capture quality for an entire session under external GPU pressure
- **File:** `llviewertexture.cpp:764-765` (entry: `driver_pressure = free < reserve`), `:796-800` + `:640-641` (recovery needs `free > 1.25×reserve` for E→N, `> 0.75×reserve` for C→E).
- **Sequence:** the governor uses **adapter-wide** free VRAM as a pressure signal. A second GPU app — OBS, a browser, an NLE, i.e. the normal machinima capture desktop — dips adapter-free below the reserve for even one 1-Hz sample → ELEVATED latches. If that app then parks free memory in `(reserve, 1.25×reserve]`, observed pressure is Normal but `recovery_headroom` is false every frame → **latched ELEVATED indefinitely** → capture pin and both full-res sites stay off, bias floored at 1.5 — even though the viewer's own `cap_usage` is tiny and nothing it discards can free another process's memory. On an 8 GB card the trap band is 819–1024 MB adapter-free.
- **Impact:** capture quality can silently never engage while recording alongside OBS — the exact intended use case.
- **Correction (policy):** during capture mode, treat the driver signal as advisory and let the viewer's own `cap_usage` be authoritative for recovery; or narrow/making-exitable the driver recovery band; or surface the driver-pinned condition in the status line so the operator knows why quality is held.

### P2-2 — Boost-pinned mip-bearing residual can make Pressure→Normal unreachable under a small custom cap
- **File:** `llimagegl.cpp:132-141` (`mMipmappedBytes = estimated` for any mip-bearing record) feeding `sReducibleTextureVRAMMegabytes`; exclusions at `llviewertexture.cpp:3446-3451` (`mDontDiscard`), `:3509-3514` (baked-avatar / high-boost `scaleDown` skip).
- **Sequence:** "reducible" counts every mip-bearing allocation, including textures discard cannot shrink (`mDontDiscard` LOD, baked avatar, full-res HDRI). If that pinned subset alone exceeds 0.80 × `max(effective−fixed, 1)` — realistic at a 768 MB–1 GB custom cap in a crowded region — `cap_usage` can never cross the strict 0.80 recovery threshold → permanent ELEVATED → capture quality permanently off. The cap genuinely *is* blown here; the defect is that nothing surfaces "the irreducible reducible floor exceeds the recovery threshold" to the operator.
- **Correction:** classify known-pinned bytes (boost ≥ HIGH, `mDontDiscard`) as *fixed* rather than *reducible*, or add a status warning when the pinned floor blocks recovery.

### P2-3 — Pre-existing accounting undercount: two pipeline 3D textures bypass the wrappers (~1.25 MB)
- **File:** `pipeline.cpp:2592` (`mCGLut`, color-grading LUT) and `pipeline.cpp:2791` (`mProjVolDustMap`, ~1 MB 64³ RGBA8 volume) call raw `glTexImage3D` with no `alloc_tex_image`/`setManualImage` pairing; their `deleteTextures` cleanups are no-ops against the accounting map.
- **Impact:** ~1.25 MB invisible to the governor. Not introduced by this change; surfaced per the packet's "list any path that bypasses the wrappers."
- **Correction:** `LLImageGLMemory::alloc_tex_image(w,h,fmt,1,false)` after each successful upload (both single-level).

### P3 (minor / informational)
- **Dead-band bias high-water freeze** (`llviewertexture.cpp:840/861` ramp gate, `:883` decrement gate): with `cap_usage` parked in `[0.80, 0.85)` and latched ELEVATED, bias neither ramps nor decays — it freezes at its prior peak (up to 4.0) until usage drops below 0.80. Self-limiting (high bias pushes usage down); minor residual over-discard.
- **One-frame pin/full-res skew** on the RAM signal: `BDMergeMemoryBudget::refresh()` is also driven by the tex/mesh pools, so `isCritical()` can flip between the bias-pin decision (`:950`) and the full-res gates (`:2112/:3428`) within a frame — cosmetic, self-correcting; the VRAM cap cannot be defeated (only `updateClass` writes `sVRAMPressureState`).
- **Draw-distance shrink during a take** (`llviewerdisplay.cpp:235-237`, bias > 2 at latched CRITICAL): intended governor ownership, but a visible mid-shot change; the "CAP ENFORCED DURING CAPTURE" title does surface the state.
- **`postBuild` commit-on-open** (`llfloaterpreferencesgraphicsadvanced.cpp:118-119`): self-writes `RenderMaxVRAMBudget` to its current value — harmless (no numeric change).
- **Dead code** `sFreezeImageUpdates` (only ever cleared) — pre-existing.
- **Scenario-12 doc wording:** the 0.92/latched-CRITICAL case correctly ramps bias (0.92 ≥ 0.85 is genuine observed pressure); the design doc/scenario text should say so explicitly rather than implying a floor hold.

## 3. Scenario matrix (all 20)

| # | Scenario | Call |
|---|---|---|
| 1 | Cold start pre-detection | PASS (bias init 1.f; `detected_vram` floored 768) |
| 2 | Auto divisor 0 / 1 / extreme | PASS (`max(divisor,1)`) |
| 3 | Custom 768 / exact / stale-above | PASS (`llclamp(768..detected)`) |
| 4 | Reducible 84/85/94/95/90/89/80/79 | PASS (inclusive boundaries; latch fixed) |
| 5 | Driver telemetry unavailable all session | PASS (`has_driver_headroom=false`) |
| 6 | NVIDIA free crossing reserve / half | PASS (P2-1 dead-zone caveat) |
| 7 | Eviction inc / unchanged / reset / wrap | PASS (`n/a` + `-1` guard) |
| 8 | ATI valid / zero / failed / implausible | PASS (`-1` sentinel + `glGetError`) |
| 9 | Low sysmem + low GPU | PASS (float factor; recoverable once sysmem clears) |
| 10 | Pressure → minimize → Critical → restore | PASS (floor reasserted after restore) |
| 11 | One-frame Critical spike instantly resolves | PASS (bias ≤ 2.5, never approaches 4.0) |
| 12 | Hover in each hysteresis dead band | PASS (0.83 holds at high-water P3; 0.92 correctly ramps as observed pressure) |
| 13 | Two viewers / adapter-wide exhaustion | PASS logic; P2-1 dead-zone applies |
| 14 | Fixed allocations above small custom cap | PASS (`effective = fixed+256`; P2-2 pinned-residual caveat) |
| 15 | GL init / context loss / shutdown | PASS (`mInited` gate + `-1` sentinel) |
| 16 | Generic compressed + DXT ± mips | PASS (4/8 bpp; correct pool) |
| 17 | RT color/depth OOM after tracker insertion | PASS (`free_cur_tex_image` + `deleteTextures`, increment after check) |
| 18 | Capture begins during Critical | PASS (overrides suppressed, UI reports enforcement, resumes only at Normal) |
| 19 | Cubemap faces repeatedly init + deleted | PASS (count-6 reclassify; idempotent free) |
| 20 | 4096×1 mip texture + no-mip noise/depth | PASS (chain terminates both dims=1; no-mip → fixed) |

## 4. Accounting assessment

- **Counted once:** GL textures incl. render-target textures (`getTextureBytesAllocatedEstimate`) + vertex buffers → `tracked`. `reducible` = mip-bearing subset; `fixed = max(tracked−reducible, 0)`. `reducible ≤ tracked` and `fixed ≥ 0` by construction (`mMipmappedBytes` is 0 or == the record's estimate).
- **No double-count:** `LLRenderTarget::sBytesAllocated` feeds display-only `sRenderTargetVRAMMegabytes`; never added to `tracked`.
- **Estimated (not exact):** compressed at 4/8 bpp; mip chain as the geometric sum; depth at 24 bpp (driver ~32).
- **Undercounted:** `pipeline.cpp` `mCGLut` + `mProjVolDustMap` (~1.25 MB, P2-3, pre-existing); CEF browser textures via raw `glGenTextures` (separate subsystem, pre-existing).
- **Classification caveat (P2-2):** boost-pinned / `mDontDiscard` mip-bearing bytes are counted as *reducible* though discard can't shrink them.
- `MIP_CHAIN_SCALE` removed (zero matches); `setManualImage` accounts only at miplevel 0; RT-OOM paths free the still-bound failed texture and delete the id; legacy 6-face cubemaps reclassify to count-6; all 22 `setManualImage`/`alloc_tex_image` call sites thread the correct mip-ness (header default `false` = safe undercount direction).

## 5. State-machine proof

- **Entry:** `is_sys_low || eviction || cap_usage≥0.95 || driver_critical` → CRITICAL; `cap_usage≥0.85 || driver_pressure` → ELEVATED. Escalation is immediate and multi-rung (`:784-788`).
- **Recovery:** one rung per independent 5 s; loose 0.90 gate for CRITICAL→ELEVATED, strict 0.80 for ELEVATED→NORMAL; `pending_recovery_target` resets the timer on target change (no stale elapsed reuse). No governor-internal latch: every input that genuinely clears the strict thresholds reaches NORMAL (`scaleDown` really shrinks the reducible counter). The only non-recovering inputs are P2-1 (external driver) and P2-2 (pinned residual) — both real-resource, not logic latches.
- **Bias ramp/decay:** ramp gated on **observed** pressure (`observed_is_low`, `:840/:861`); decay in the Normal-observed branch. So a resolved spike steps the floor down 2.5→~1.5→1.0 rather than ramping to 4.0 (the sibling tree's P2-A is fixed here).
- **Floor / background:** per-frame floor (2.5/1.5, ×sysmem factor) follows the **latched** state; runs after the background save/restore, so restoring a pre-pressure bias cannot defeat an active floor. Final clamp `[1,4]`.
- **Capture ownership transfer:** pin (`sDesiredDiscardBias=1.f`) and both full-res sites gate on the **latched** `sVRAMPressureState`, so quality yields the frame pressure is observed and resumes only after the state demotes to NORMAL — with capture still armed and the RAM pool not critical.

## 6. Platform assessment

- **NVIDIA (NVX):** current-free + eviction handled in KiB/adapter-wide; 1-Hz gate; `-1` sentinel + `glGetError`; `mInited` gate. Correct. P2-1 driver dead-zone is the main behavioral risk.
- **AMD (ATI_meminfo):** free-memory as coarse pressure; no eviction (shows `n/a`); `-1` sentinel makes a failed/unsupported query yield "unavailable," never a false 0-free Critical.
- **Fallback (no extension):** resolves to unavailable; governor runs on viewer accounting only.
- **Multi-client:** driver values are adapter-wide → two viewers see each other's usage as pressure (same mechanism as P2-1); logic sound, tuning is runtime.

## 7. UI/settings assessment

- Slider in `floater_preferences_graphics_advanced.xml` with `control_name="RenderMaxVRAMBudget"`, range 0..detected, 0 = Auto; float→U32 normalized in `onVRAMCapChanged` (round + floor 768 + `setU32`); Apply/Cancel integrity holds (opening does not numerically change the value). Governor reads the U32 live each frame (no stale typed cache).
- 3-state status title is mutually exclusive by truth table; "CAP ENFORCED DURING CAPTURE" shows when capture is armed under pressure; status lists tracked/effective/state/reducible/fixed/driver ("Unavailable", never negative) + the fixed-floor adjustment.
- Layout hand-traced: floater 825×632; the VRAM status border (top 500, h 70) sits clear of the tone-mapping sliders (end ≈491), VSync, Avatar heading, and bottom buttons (start ≈576) — no overlaps.
- Texture console: 9 distinct rows, RT labeled `Render(subset)` and excluded from Total, `n/a` for unavailable driver/eviction. Note: status is text-only (no graphical cap bar in either surface) — not claimed, informational.
- `llformat` specifiers all match argument types; both edited XMLs parse.

## 8. Runtime gaps (smallest useful hardware plan)

1. **Capture + OBS (P2-1):** record with OBS open; confirm whether capture quality engages, or whether the driver dead-zone holds ELEVATED — decide the policy from the observed behavior.
2. **Small custom cap in a crowded region (P2-2):** set 768 MB–1 GB; confirm Pressure→Normal is reachable, or that the pinned residual blocks it.
3. **On-camera spike (scenario 11):** spike usage ≥0.95 then release; confirm bias does not pin at 4.0 and quality resumes at Normal.
4. **AMD + forced-failing query:** confirm "Unavailable," not Critical.
5. **RT OOM:** force a render-target allocation failure; confirm no id/byte leak.
6. **8–12 GB card:** confirm discard engages early enough given the divisor-1 / per-record-mip defaults.

## 9. Final recommendation

- **Merge-ready.** The capture-mode cap-enforcement bug is fixed and verified; the build is real and SHA-matched; no P0/P1 regressions.
- **Decide before relying on capture under load:** the P2-1 driver-pressure-during-capture policy (advisory vs. authoritative) — this is the one most likely to affect the OBS-alongside workflow — and optionally surface the P2-2 pinned-floor condition. Both are behavioral choices, not bug fixes.
- **Trivial cleanup:** the ~1.25 MB `pipeline.cpp` 3D-texture undercount (P2-3), pre-existing.
