# Claude Adversarial Review: VRAM Governor Frame Pacing

## Reviewer instructions

Perform a deep, adversarial, read-only review of the VRAM-governor frame-pacing
changes in `I:\alchemy-machinima`. Do not edit, format, configure, build, run,
commit, or revert anything. The working tree contains unrelated work; restrict
the review to the files and behaviors named below.

Treat this document as a list of claims to prove or disprove from source, not as
evidence. Report concrete findings first, with severity, exact file and line,
trigger sequence, impact, and a minimal correction. Explicitly say when a claim
requires runtime evidence.

This is the Alchemy Machinima client. The governor is controlled by
`RenderVRAMGovernorEnabled`; Off is required to retain the legacy scheduling
path. Do not assume DXGI, Firestorm, or Black Dragon implementation details.

## Review scope

Primary implementation:

- `indra/newview/llviewertexture.cpp`
- `indra/newview/llviewertexturelist.cpp`
- `indra/newview/llviewertexture.h`
- `indra/newview/llviewertexturelist.h`
- `indra/newview/llviewerdisplay.cpp`

UI remediation included in this pass:

- `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`

Related policy that must not regress:

- `indra/newview/app_settings/settings.xml`
- `indra/newview/llfloaterpreferencesgraphicsadvanced.cpp`
- capture-quality policy and every consumer of `sDesiredDiscardBias`

The earlier accounting system is not being redesigned in this pass. Review its
interaction with pacing where necessary, but do not re-report previously noted
estimation-only items unless this change makes one worse.

## Intended outcome

The old cap-only Critical transition synchronously called
`updateImageDecodePriority()` for every texture on the main/render thread. Small
per-frame increases in discard bias also reset `sBiasTexturesUpdated` every
frame, continually rearming an aggressive sweep. Together these could cause a
visible FPS hitch each time the governor discarded, followed by a refill and
another hitch.

The revised design should:

1. Send ordinary configured-cap pressure through a paced texture sweep.
2. Keep an immediate synchronous safety purge only for low system memory or a
   newly confirmed NVIDIA driver eviction.
3. Start cap pressure at the off-screen bias of 1.5 and limit cap-only bias
   attack to 0.75 bias units per second.
4. Restart an accelerated sweep only after a meaningful 0.25 bias increase.
5. Bound the added candidate count and best-effort CPU time per frame.
6. Count entries actually reached before the time-budget break, rather than all
   entries merely requested for that frame.
7. Delay and slow quality restoration so the viewer does not immediately refill
   VRAM and cross the pressure threshold again.
8. Leave Governor Off on the legacy update-count, timing, face-check, and
   every-bias-increase reset behavior.
9. Give the Enable checkbox its own readable and clickable row below live
   status text.

## Implementation claims to verify

### 1. Cap pressure versus emergencies

Inspect `LLViewerTexture::updateClass()` around
`llviewertexture.cpp:688-953`.

- `emergency_pressure` is true only for `isSystemMemoryLow()` or a newly
  detected driver eviction (`llviewertexture.cpp:786`).
- Crossing the viewer's 95% configured-cap threshold, or driver free-memory
  headroom alone, may enter Critical but must not execute the full-list loop.
- Cap-only Critical enters at bias 1.5 and logs that a paced sweep was queued.
- Emergency pressure raises bias immediately and executes exactly one
  synchronous full-list scan per system-low episode or eviction event
  (`llviewertexture.cpp:892-905`).
- A persistent system-low signal must not execute the synchronous scan every
  frame. A later distinct driver-eviction increment must be able to trigger a
  new emergency scan.
- The legacy Governor-Off branch retains its original low-memory/emergency
  behavior; the new enabled path must not accidentally disable that safeguard.

Adversarially test the state truth table. In particular, inspect:

- cap-only Normal -> Critical;
- driver-critical without an eviction increment;
- cap Critical followed later by an eviction increment;
- eviction pulse while already Critical;
- system-low true for many frames, then false, then true again;
- disabling and re-enabling the governor during each case.

### 2. Bias attack and visible-texture protection

Inspect `llviewertexture.cpp:869-922` and the final floor at
`llviewertexture.cpp:1098-1114`.

- Elevated begins at 1.5, which applies downrez pressure to off-screen textures.
- Cap-only Critical does not jump directly to 2.5. After the existing one-second
  evaluation delay, cap-only growth is capped at 0.75 bias units per second.
- The latched-state floor remains 1.5 and therefore cannot defeat the paced
  attack by forcing Critical immediately to 2.5.
- A real emergency is not rate-limited below its immediate 2.5 response.
- `llviewertexturelist.cpp:904-1023` still uses frustum and
  `mImportanceToCamera` rules; camera-important visible textures remain favored.
- With the governor active and bias above 1, a single decode-priority call checks
  at most 256 faces instead of 1024 (`llviewertexturelist.cpp:919-1008`). If a
  texture exceeds the limit, it is conservatively boosted rather than
  incorrectly downrezzed.
- Capture mode cannot force bias to 1 or request full resolution while the
  latched state is Pressure/Critical.

Quantify the expected timing at 30, 60, and 120 FPS. The rate must be based on
elapsed frame time, not frames, and should remain approximately frame-rate
independent.

### 3. Sweep reset and accounting

Inspect `llviewertexture.cpp:1125-1148` and
`llviewertexturelist.cpp:1241-1351`.

- With the governor enabled, `sBiasTexturesUpdated` resets only when discard
  bias has risen at least 0.25 above the last sweep-reset bias.
- Bias increases smaller than 0.25 must not reset the counter every frame.
- A 1.0 -> 1.5 entry jump and later 0.25 steps must each schedule a fresh pass.
- Bias reduction must update the tracking high-water mark without spuriously
  resetting the sweep.
- The candidate multiplier is bounded to no more than the normal candidate
  count plus 64 when enabled.
- The accelerated path uses `min(existing_budget, 0.001 seconds)` as a
  best-effort per-frame work budget.
- Candidate-vector preparation is included in elapsed time when enabled. A
  single texture update is not preemptible, so 1 ms is a target rather than a
  hard real-time guarantee.
- `processed_count` advances only for entries the processing loop actually
  reaches before its break. It must not advance by the requested candidate
  count.
- The counter clamps to the current map size and does not overflow, stick in
  accelerated mode permanently, or declare completion immediately after a
  mostly unprocessed candidate batch.

Attack dynamic-container cases: map growth, map shrink, zero textures, entries
without GL textures, deletion as a side effect of priority/update work, and a
timer budget exceeded during candidate copying or the first texture.

### 4. Recovery stability

Inspect `llviewertexture.cpp:847-953`.

- Existing pressure recovery remains one rung per independent five-second
  interval: Critical -> Pressure below 90%, then Pressure -> Normal below 80%.
- Returning to Normal starts a separate three-second quality-recovery hold.
- Any renewed observed pressure cancels that hold.
- Bias cannot decline until the latched state is Normal, the hold has elapsed,
  reducible use is below 78%, driver/system headroom is sufficient, and bias is
  above 1.
- Governor quality recovery is limited to 0.025-0.10 bias units per second,
  based on the existing setting but capped for pacing.
- Bias cannot underflow below 1 because the existing final clamp remains.
- The combination of the 78% gate, three-second hold, and slow decay should
  create headroom below the 85% entry threshold rather than refill directly
  into another discard cycle.

Walk a one-frame Critical spike and sustained Critical episode frame by frame.
Check for stale timer reuse, a hold that never expires, recovery while still
latched Elevated, and quality recovery that restarts during pressure.

### 5. Governor-Off compatibility

This is a release invariant, not a cosmetic preference.

- Driver telemetry/state-machine work remains skipped when Off.
- `llviewertexture.cpp:1135-1139` retains the legacy reset on every bias
  increase.
- `llviewertexturelist.cpp:1276-1281` retains the original full multiplier and
  approximate pre-increment of `sBiasTexturesUpdated`.
- `llviewertexturelist.cpp:1307-1312` resets the timer after candidate copying,
  matching the legacy timing scope.
- The face limit remains 1024 when Off.
- Enabling and disabling live must not leave an impossible stale counter, bias,
  emergency edge, or recovery-hold state.

Compare the Off path to the pre-governor behavior rather than merely checking
that it compiles.

### 6. UI layout remediation

Inspect
`indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`.

- Floater height is 672 (`:3`).
- The VRAM status border has dedicated expanded space (`:1196-1204`).
- `VRAMStatus` has 58 units of wrapped-text height (`:1218-1230`).
- `VRAMGovernorEnabled` occupies its own 20-unit row at top 580
  (`:1231-1242`), leaving a 16-unit gap after the status text.
- The bottom divider moves to top 614 (`:1244-1251`), with the bottom buttons
  remaining inside the floater and clear of the governor panel.
- Check common UI scales and long translated/fallback text. No status text may
  cover the checkbox or reduce its clickable area.

## Required scenario matrix

Mark each item Pass, Fail, or Runtime required and explain why.

1. Governor Off with legacy VRAM pressure and background/minimize behavior.
2. Governor On, cap use crosses 85% but remains below 95%.
3. Governor On, cap use jumps directly from below 85% to above 95%.
4. Pressure rises slowly from 85% through 95% while bias changes every frame.
5. Cap-only Critical with 1,000, 10,000, and 100,000 tracked textures.
6. System-low Critical lasting 300 frames.
7. One NVIDIA eviction increment, no change, counter reset/wrap, then another
   increment.
8. Driver free memory below half-reserve without an eviction increment.
9. Critical clears below 90%, then below 80%, then hovers at 78-85%.
10. Pressure returns during the three-second quality hold.
11. Bias reaches 4, recovers to Normal, then restores quality.
12. Capture begins in Normal and pressure enters during the take.
13. Capture begins while already Critical.
14. Minimize/background during cap pressure, then restore the window.
15. Empty texture map and a map dominated by entries without GL textures.
16. A texture referenced by more than 256 faces, including one visible to the
    camera and one off-screen.
17. One `updateImageDecodePriority()` call exceeds one millisecond by itself.
18. Live governor disable/re-enable during cap Critical and system-low Critical.
19. UI at 100%, 150%, and 200% scale with a three-line status string.
20. Repeated fill -> discard -> recover cycles; compare frame-time spikes and
    cycle frequency with the previous implementation.

## Known limitations and non-claims

Do not report these as surprises, but assess whether their tradeoff is safe:

- The one-millisecond scheduler budget is best-effort. Individual texture work
  cannot be preempted; the 256-face guard reduces but does not eliminate that
  risk.
- A real system-low or confirmed driver-eviction event can still hitch because
  safety deliberately retains a synchronous full-list scan.
- NVIDIA/ATI free-memory queries remain synchronous and sampled at most once per
  second. This pass does not make those OpenGL queries asynchronous.
- This pass rate-limits quality restoration through bias and the existing
  texture scheduler. It does not implement a byte-accurate GPU-upload token
  bucket.
- Recovery from bias 4 to bias 1 can intentionally take about 30 seconds after
  the state-machine and three-second hold. This favors shot stability over fast
  texture sharpening.
- Hardware frame-time improvement and VRAM-loop suppression require in-world
  runtime measurement; source inspection alone cannot prove them.

## Static verification already performed by Codex

On 2026-08-31:

- Scoped `git diff --check` completed without whitespace errors; it emitted only
  the existing line-ending warning for `llviewertexture.cpp`.
- The final modified XUI parsed successfully with PowerShell's XML parser. Its
  geometry was also asserted: status bottom 564, toggle 580-600, panel bottom
  608, divider 614.
- Source inspection confirmed that cap-only Critical no longer owns a full-list
  loop and that the remaining enabled-path full-list loop is guarded by the
  emergency edge.
- No build or viewer runtime test was performed for this pass.

## Suggested static commands

```powershell
git status --short
git diff --check
git diff -U12 -- indra/newview/llviewertexture.cpp indra/newview/llviewertexturelist.cpp
git diff -U12 -- indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml
rg -n -S "RenderVRAMGovernorEnabled|sDesiredDiscardBias|sBiasTexturesUpdated|emergency_pressure|quality_recovery_hold|MAX_PACED_SWEEP" indra/newview
rg -n -S "for \(auto& image : gTextureList\)|updateImageDecodePriority\(" indra/newview/llviewertexture.cpp indra/newview/llviewertexturelist.cpp
[xml](Get-Content indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml -Raw) | Out-Null
```

## Required response format

1. **Verdict:** Ready, Ready with risks, or Not ready.
2. **Findings:** ordered P0, P1, P2, P3; exact source anchors and trigger
   sequences. State "No findings" for empty severities.
3. **Claim audit:** Pass/Fail/Runtime required for every claim section above.
4. **Scenario matrix:** all 20 scenarios.
5. **Governor-Off parity assessment.**
6. **Capture interaction assessment.**
7. **UI layout assessment.**
8. **Runtime gaps and the smallest useful NVIDIA/AMD test plan.**
9. **Final recommendation:** merge, remediate first, or accept named risks.
