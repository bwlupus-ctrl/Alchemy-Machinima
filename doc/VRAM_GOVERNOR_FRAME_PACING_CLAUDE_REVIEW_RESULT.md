# Claude Adversarial Review — RESULT: VRAM Governor Frame Pacing

Read-only source review of the uncommitted changes in `llviewertexture.cpp`,
`llviewertexturelist.cpp`, and `floater_preferences_graphics_advanced.xml`.
No build/run performed. Line anchors are current-tree.

## 1. Verdict

**Ready with risks.** The pacing redesign is correct and the Governor-Off path is
a faithful legacy reproduction. No P0/P1 defects found. Two low-severity behavior
gaps (one is a stated-claim shortfall) and a mandatory build gate remain.

## 2. Findings

### P2-1 — A driver eviction during a sustained system-low episode is masked
`llviewertexture.cpp` emergency scan is gated on the rising edge of a single
combined flag: `if (emergency_pressure && !was_emergency_pressure) { …full-list
scan… }`, `emergency_pressure = is_sys_low || eviction_detected`.
- Trigger: system-low true for many frames (`was_emergency_pressure` latched
  true) → a distinct NVX eviction increment arrives → `eviction_detected` pulses
  but `emergency_pressure` is already true and `was_emergency_pressure` is true →
  **no new synchronous scan**.
- Impact: violates review claim §1 "a later distinct driver-eviction increment
  must be able to trigger a new emergency scan." Practical risk is low because
  bias is already ramping during system-low, but the immediate eviction rescan is
  skipped for the overlap.
- Minimal fix: gate on independent edges, e.g.
  `const bool run_emergency_scan = (is_sys_low && !was_sys_low) || eviction_detected;`
  (`eviction_detected` is already a one-frame edge by construction), tracking
  `was_sys_low` separately. Keep `was_emergency_pressure` only for logging if
  desired.

### P3-1 — `was_low` (Governor-Off) can be stale across an On→Off transition
The legacy `static bool was_low` is written only inside the Off branch. After a
period with the governor On, the first Off frame compares against a stale
`was_low`. If the tree re-enters Off while already low with `was_low==true`, the
legacy "slam to 1.5 + emergency purge" rising-edge is skipped for that episode.
- Impact: cosmetic/negligible — the legacy per-frame ramp still runs and the
  governor typically left bias elevated. One missed slam edge on live re-disable.
- Minimal fix: on the governor-enabled branch, also refresh `was_low =
  (isSystemMemoryLow() || over_pct>0)` equivalent, or reset `was_low=false` when
  leaving the On branch so the first Off frame re-arms the edge.

### P3-2 (note, not a defect) — `driver_critical` without eviction is paced, not immediate
`driver_critical` (driver headroom < half reserve) raises `observed_pressure` to
Critical but is **not** in `emergency_pressure`, so it enters at bias 1.5 and
attacks at ≤0.75 bias/s. This is intentional per §1 ("driver headroom alone may
enter Critical but must not execute the full-list loop"), but means a genuine
near-OOM that is not yet evicting relies on the slower ramp until an actual
eviction fires the emergency path. Acceptable tradeoff; flag for runtime on a
real low-VRAM NVIDIA card.

### Build gate
No build was performed. The code reads compile-clean on inspection (all referenced
symbols — `EVRAMPressureState`, `getSystemMemoryBudgetFactor`, `isSystemMemoryLow`,
`getFreeSystemMemory`, `BDMergeMemoryBudget::isCritical`, `S32Megabytes` — are
pre-existing), but a Release build is required before merge.

## 3. Claim audit

- **§1 Cap pressure vs emergencies — PASS with P2-1 exception.** Cap-only Critical
  enters at 1.5 and logs "queuing a paced texture downrez sweep"; no full-list
  loop. Emergency full-list scan runs once per rising edge of `emergency_pressure`
  (`emergency && !was_emergency`). Persistent system-low does NOT rescan every
  frame ✓. Distinct eviction during overlap is masked (P2-1). Off branch retains
  its own low-memory/emergency purge ✓.
- **§2 Bias attack & visible-texture protection — PASS.** Elevated=1.5; cap-only
  Critical does not jump to 2.5 (`minimum_bias=1.5` unless `emergency && CRITICAL`);
  paced increment `= emergency ? increment : min(increment,0.75)`, `*
  gFrameIntervalSeconds` (frame-rate independent). Latched floor is 1.5 for any
  non-Normal state (not 2.5). Emergency floor 2.5 not rate-limited. Face cap
  256 when `governor && bias>1` else 1024; `face_count>limit ⇒ max_vsize=MAX_IMAGE_AREA`
  (conservative boost, not downrez) ✓. Capture cannot force bias=1 during pressure
  (see §6). `mImportanceToCamera`/frustum boost preserved ✓.
- **§3 Sweep reset & accounting — PASS.** Governor resets `sBiasTexturesUpdated`
  only when `last_bias + 0.25 <= bias`; sub-0.25 rises don't reset. Bias decrease
  updates `last_texture_update_count_bias` without resetting the sweep. Candidate
  count bounded to `min(accelerated, normal+64)` when enabled. Budget
  `min(max_time,0.001)`; candidate-copy included in elapsed when enabled (timer
  declared before copy, not reset). `processed_count` advances per entry reached
  before break, then `sBiasTexturesUpdated = min(prev+processed, mUUIDMap.size())`
  — clamps to map size, no overflow, completion logged on crossing ✓.
- **§4 Recovery stability — PASS.** State recovery one rung / 5s (Critical→Elevated
  <90%, Elevated→Normal <80%). Normal entry starts a 3s hold; any observed
  pressure sets `quality_recovery_hold_active=false`. Bias decrement gated on
  `state==Normal && !hold && bias>1 && cap_usage<0.78 && driver headroom && free
  sys mem`, clamped 0.025–0.10/s. Final `llclamp(...,1,4)` prevents underflow.
  78%/hold/decay create headroom below the 85% entry ✓.
- **§5 Governor-Off compatibility — PASS.** Driver telemetry + state machine are
  inside `if (vram_governor_enabled)`. Off branch reproduces pre-governor logic
  (over_pct, `was_low` edge, purge on `sys_low || over_pct>2`, ramp up/down),
  resets `sDriverAvailableVRAMMegabytes=-1`, `sVRAMPressureState=NORMAL`,
  `was_emergency_pressure=false`, `quality_recovery_hold_active=false`. Legacy
  every-bias-increase reset and full multiplier + pre-increment retained on the
  Off path. Face limit 1024 when Off ✓. (Minor staleness: P3-1.)
- **§6 UI layout — PASS.** Floater 672; border top 480 h128 (bottom 608); status
  top 506 h58 (bottom 564); checkbox own row top 580 h20 (16px gap after status,
  inside border); divider top 614; buttons use `top_delta` relative to the divider
  → 621–644, inside the floater, clear of the panel. XML parses. No status text
  overlaps the checkbox.

## 4. Scenario matrix

1. Off + legacy pressure/background — **Pass** (legacy branch faithful; background
   floor block is governor-gated but the Off path never set the pressure floor
   originally).
2. On, 85–95% — **Pass** (Elevated, bias≥1.5, paced).
3. On, <85%→>95% direct — **Pass** (Critical, cap-only ⇒ enter 1.5 then ≤0.75/s,
   no full-list loop).
4. Slow 85→95, bias changes each frame — **Pass** (0.25-step sweep reset stops the
   per-frame rearm).
5. Cap-only Critical @1k/10k/100k textures — **Pass** (no synchronous full scan;
   paced sweep bounded by +64 and 1ms; **Runtime** to confirm 1ms holds at 100k).
6. System-low Critical 300 frames — **Pass** (one scan on the edge, not per frame).
7. One eviction, reset/wrap, another — **Pass** for isolated evictions;
   `last_eviction_count` re-baselines on counter reset (`eviction>last`), so a
   wrap-to-lower value simply re-baselines without a false positive.
8. Driver <½ reserve, no eviction — **Pass** (Critical, paced, no full loop) — see
   P3-2 note.
9. Clears <90%, <80%, hovers 78–85% — **Pass** (stable; no decrement until <78%).
10. Pressure returns during 3s hold — **Pass** (`quality_recovery_hold_active=false`
    on any observed pressure).
11. Bias 4 → Normal → restore — **Pass** (slow 0.025–0.10/s; ~30s intended).
12. Capture starts Normal, pressure enters — **Pass** (pin yields via
    `!isVRAMPressureActive()`).
13. Capture starts already Critical — **Pass** (pin inactive; governor owns bias).
14. Minimize/background during pressure, restore — **Pass** (background max-bias
    then restore; governor floor reasserts while pressure observed).
15. Empty map / all-no-GL entries — **Pass** (`processed_count=0`⇒counter
    unchanged; boost path needs a GL texture; empty loop safe).
16. >256-face texture, on- and off-screen — **Pass** (boosted to MAX_IMAGE_AREA;
    on-screen important face never wrongly downrezzed even if beyond the cap).
17. One `updateImageDecodePriority` >1ms — **Runtime** (acknowledged: not
    preemptible; 256-face guard mitigates; single-texture overrun possible).
18. Live disable/re-enable during cap Critical & system-low — **Pass** (Off branch
    resets governor state; **P3-1** minor `was_low` edge).
19. UI 100/150/200% with 3-line status — **Runtime** (58px status ≈ 3 lines at
    100%; **verify 150/200% and long localized strings don't push the checkbox**).
20. Repeated fill→discard→recover — **Runtime** (the whole point; needs frame-time
    capture vs previous build).

## 5. Governor-Off parity

Strong. The Off branch is a self-contained reproduction of the pre-governor
discard-bias management (divisor default 2 via a distinctly-named cached control to
avoid shadowing; `over_pct` target math; `was_low` rising-edge slam+purge; ramp
up/down with the original increments) and explicitly clears all governor state on
entry. The only gap is P3-1 (stale `was_low` across On→Off), which is negligible.
`sFreeVRAMMegabytes`/effective-cap accounting is intentionally kept live in both
modes so Preferences reads consistently — reasonable, not a regression.

## 6. Capture interaction

Correct. `isCaptureQualityPinActive() = isCaptureModeActive() && !isVRAMPressureActive()`
and `isVRAMPressureActive() = sVRAMPressureState != NORMAL`. During Elevated/Critical
the pin is inactive, so the `sDesiredDiscardBias = 1.f` line does not run and cannot
defeat the pressure floor. With the governor Off, `sVRAMPressureState` is pinned
Normal, so capture pins full quality as the checkbox tooltip promises. Ordering is
safe: pressure floor → capture pin (gated) → final clamp.

## 7. UI layout assessment

No overlap; buttons follow the divider via `top_delta`; checkbox has its own
clickable 20px row with a 16px gap above. Only open item is high-DPI / long-string
robustness (scenario 19) — recommend a quick 150%/200% visual check.

## 8. Runtime gaps & smallest useful test plan

Source cannot prove: the 1ms budget under a single heavy `updateImageDecodePriority`,
actual frame-time smoothing vs the old cap-only hitch, and DPI/localized layout.
Smallest NVIDIA/AMD plan:
- On an NVIDIA card, set `RenderMaxVRAMBudget` low enough to force cap Critical in a
  dense region; capture frame-time (Tracy or the built-in timers) across a
  fill→discard→recover cycle with governor Off vs On; confirm the On curve loses the
  periodic hitch.
- Force a real eviction (over-commit VRAM) to confirm the emergency scan still fires
  once and, importantly, **retest P2-1**: trigger system-low first, then an
  eviction, and confirm whether the eviction rescan is needed in practice.
- UI: open the floater at 150% and 200% scale.

## 9. Final recommendation

**Remediate P2-1, then merge with named risks.** P2-1 is a one-line edge-split that
restores the stated eviction-safety guarantee; worth doing before merge. P3-1 is
optional. Then build Release (mandatory) and run the small NVIDIA plan. The
best-effort 1ms budget and the intentional ~30s bias-4→1 recovery are acceptable
documented tradeoffs.
