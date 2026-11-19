# Claude Re-review: VRAM Governor Frame-Pacing Remediation

## Reviewer instructions

Perform a focused, adversarial, read-only re-review in
`I:\alchemy-machinima`. Do not edit, format, configure, build, run, commit, or
revert anything. Verify the two remediations below from source and check that
they do not regress the previously accepted pacing design.

The original review result is:

- `doc/VRAM_GOVERNOR_FRAME_PACING_CLAUDE_REVIEW_RESULT.md`

The original full review packet is:

- `doc/VRAM_GOVERNOR_FRAME_PACING_CLAUDE_REVIEW.md`

Treat all claims in this packet as hypotheses to prove or disprove.

## Remediation summary

Claude's review reported:

1. **P2-1:** a single combined `was_emergency_pressure` edge masked a new
   driver eviction while system-low remained active.
2. **P3-1:** the legacy branch's local `was_low` could remain true across a live
   Off -> On -> Off cycle, causing the second Off transition to miss its initial
   low-memory slam.

Both issues are now remediated in:

- `indra/newview/llviewertexture.cpp`

No pacing thresholds, accounting formulas, capture policy, UI geometry, or
texture scheduler limits were changed during this remediation.

## Fix 1: independent system-low and eviction triggers

### Previous defect

The emergency purge used:

```cpp
emergency_pressure && !was_emergency_pressure
```

where `emergency_pressure = is_sys_low || eviction_detected`. If system-low was
already true, the combined edge stayed true and a later independent eviction
could not trigger another safety scan.

### Current implementation

Inspect `indra/newview/llviewertexture.cpp:697` and `:897-914`.

- Persistent edge state now tracks only `was_system_memory_low`.
- `system_low_started` is `is_sys_low && !was_system_memory_low`.
- The synchronous safety scan fires on:

```cpp
system_low_started || eviction_detected
```

- `eviction_detected` is already a one-sample pulse produced only when the NVX
  eviction counter increases, so it must not receive an additional shared edge
  gate.
- `was_system_memory_low` updates after the scan decision and resets when the
  governor is disabled (`:914`, `:973`).

### Truth table to verify

| System low now | System low previous | New eviction | Safety scan |
|---:|---:|---:|:---|
| 0 | 0 | 0 | No |
| 0 | 0 | 1 | Yes: driver eviction |
| 1 | 0 | 0 | Yes: system-low rising edge |
| 1 | 0 | 1 | Yes: combined reason, exactly one scan |
| 1 | 1 | 0 | No repeated scan |
| 1 | 1 | 1 | Yes: later independent driver eviction |
| 0 | 1 | 0 | No; arm the next system-low episode |

Verify additionally:

- A sustained system-low signal cannot rescan every frame.
- Every distinct eviction-count increase can rescan, including while system-low
  remains active and while the pressure state is already Critical.
- An unchanged, reset, or wrapped eviction counter does not create a false scan
  under the existing `last_eviction_count >= 0 && current > last` rule.
- The simultaneous system-low + eviction case logs the combined reason and
  performs one loop, not two.
- Cap-only Critical and `driver_critical` without an eviction increment still
  remain on the paced path.

## Fix 2: legacy edge reset across live mode switches

### Previous defect

The Governor-Off branch owned a function-static `was_low`. When the governor was
enabled, that static retained its prior value. Switching back Off while still
low could therefore miss the legacy branch's initial 1.5 bias slam and emergency
scan.

### Current implementation

Inspect `indra/newview/llviewertexture.cpp:698`, `:702-706`, and `:1008-1034`.

- The state is now outer-scope `legacy_was_low`.
- Every enabled-governor frame clears `legacy_was_low`.
- The legacy branch uses `is_low && !legacy_was_low` for its entry work and
  stores the current `is_low` after the decision.

Verify these sequences:

1. Start Off and Normal -> become low: one legacy entry response.
2. Stay Off and low for many frames: no repeated entry scan.
3. Off/low -> On/low -> Off/still low: the second Off observes a fresh edge.
4. On/Normal -> Off/low: Off observes a fresh edge.
5. Rapid toggle while Normal: no false low-memory scan.
6. Governor On behavior does not read `legacy_was_low` for its own decisions;
   clearing it has no enabled-path side effect.

## Regression invariants

Reconfirm that remediation did not change the accepted behavior:

- Ordinary 85%/95% configured-cap pressure never executes a synchronous
  full-list scan.
- System-low rising edges and confirmed driver evictions retain immediate safety
  handling.
- Cap-only bias attack remains capped at 0.75 bias units/second.
- Accelerated sweeps restart at 0.25 bias steps, add at most 64 candidates, and
  use the best-effort one-millisecond budget.
- Recovery remains two five-second state transitions, then a three-second
  quality hold, a 78% headroom gate, and at most 0.10 bias units/second recovery.
- Capture full-quality behavior still yields whenever the latched pressure state
  is not Normal.
- Governor Off retains its legacy multiplier, approximate counter increment,
  timer scope, 1024-face limit, and per-bias-increase reset.
- The expanded VRAM panel and standalone Enable row remain staged correctly.

## Build and static evidence

Codex completed the following on 2026-08-31:

- Scoped `git diff --check`: no whitespace errors; only the existing
  `llviewertexture.cpp` line-ending warning.
- Release command:

```powershell
cmake --build build-Windows-vs2026-os --config Release --target viewer -- /m
```

- Result: **exit code 0** with MSBuild 18.7.8. Both
  `llviewertexture.cpp` and `llviewertexturelist.cpp` compiled, final link and
  viewer-manifest copy succeeded.
- Executable:
  `I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe`
- Size: `82,399,232` bytes
- Timestamp: `2026-08-31 09:00:50 -04:00`
- SHA-256:
  `D05AB541BE39FECCBEF43955D7912994031FA5CFD02413444DA4425121B2489B`
- Staged Advanced Graphics XUI matched the source byte-for-byte and parsed as
  XML. SHA-256:
  `3D06503E9463C662269ABD52C5D77081A1247196921D1048FAF6CE879FFDDBAF`
- Staged control anchors: `VRAMGovernorEnabled` at top 580 and bottom divider at
  top 614.

Build success proves compile/link/staging only. It does not prove the runtime
edge sequences or frame-time improvement.

## Minimal runtime validation still required

1. NVIDIA: induce cap-only Critical and confirm no large one-frame texture scan.
2. NVIDIA: while system-low is held true, induce a new NVX eviction increment;
   confirm a second emergency log/scan occurs once.
3. Hold system-low for several seconds without a new eviction; confirm no scan
   repeats every frame.
4. Toggle Governor Off -> On -> Off while legacy `is_low` remains true; confirm
   the second Off performs the legacy entry response.
5. Record fill -> discard -> recover frame times and compare 1% lows against the
   pre-pacing build.

## Required response format

1. **Verdict:** Ready, Ready with risks, or Not ready.
2. **P2-1 status:** Fixed or Not fixed, with a frame-by-frame proof.
3. **P3-1 status:** Fixed or Not fixed, with all six toggle sequences.
4. **Regression findings:** P0-P3 with exact file/line and trigger sequence.
5. **Build-evidence assessment.**
6. **Runtime gaps.**
7. **Final recommendation.**

