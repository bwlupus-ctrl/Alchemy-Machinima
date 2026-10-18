# Pose Polish test 23: invalid moving-history setup

2026-10-18. Fix for the failing regression introduced in `d2a79177b0f`.

## Finding

**Test 23's setup was wrong for its capture-velocity assertion.** It reads the
in-place output parameter correctly, but its 30 fps warm-up never establishes
the moving displayed history that the assertion assumes. The production capture
path already advances by tracked displayed velocity times `dt`; it does not
unconditionally output `before_pos`.

Only `indra/newview/tests/alposepolish_test.cpp` changes. No production changes,
assertion removals, assertion substitutions, tolerance increases, viewer build,
commit, or push. The production header in this checkout is
`indra/newview/alposecontinuity.h` (not under `llcharacter`).

## Production path and root cause

Trace through `ALPoseContinuity::inertialize()`:

1. The first call seeds the input/display poses and returns without measuring
   velocity (`alposecontinuity.h:185-197`). Both tracked velocities remain zero.
2. Automatic jump detection allows a positional step of
   `JUMP_POS_BASE + JUMP_VEL_SLACK * |mInputPosVel| * dt`
   (`:206-217`). At startup that is only **0.02 m**.
3. Test 23's next input at 30 fps moves **0.0333333 m**. It is classified as a
   jump, even though the fixture intends to warm up a continuous 1 m/s stream.
4. On capture, the code computes
   `target_pos = s.mLastPos + s.mDispPosVel * dt`, then
   `pos_off = target_pos - in_pos` (`:220-231`). It skips decay on that frame.
   Composition writes `io_pos = in_pos + s.mPosOffset` (`:343-346`), hence
   `io_pos = target_pos` to floating-point precision. Rotation similarly uses
   `quatExp(s.mDispRotVel * dt) * s.mLastRot`.
5. With displayed velocity still zero, this initial capture holds the seeded
   pose. Input velocity is updated only in the non-jump branch (`:273-288`),
   so each subsequent 3.33 cm warm-up step triggers capture again. The
   positional jump also captures rotation. Both displayed velocities stay zero.
6. At the actual seam assertion, `before_pos` is zero and tracked velocity is
   zero. Production computes `before_pos + 0 * dt`, correctly following the
   capture formula for the state it was given. The test incorrectly expects a
   1 m/s history and therefore a displacement of `dt`.

At 60 fps the initial 0.0166667 m step is below the 0.02 m threshold, so velocity
is learned normally. Both original 60 fps cases already behave as intended.

This demonstrates an existing cold-start jump-detection lockout for this input,
not a missing `v*dt` term or an output-parameter mix-up. Its startup detection
policy is unchanged by this narrowly scoped capture regression fix. This result
does not claim that cold-start lockout is desirable production behavior.

`ALLoopSeam::Offset` and `LLKeyframeMotion` are not called by test 23. Their
separate path captures the residual relative to the previous curve sample,
composes the authored endpoint delta, and applies it over the live current
curve. `onUpdate()` starts elapsed time at the actual wrap overshoot before
`applySeamEase()`. Nothing in that path overrides this test's in-place output.
The original diagnosis in `DIAG_anim_loop_stutter.md` predates the capture and
loop-seam changes in `d2a79177b0f`.

## Evidence before the fix

The original executable reproduced the reported failure:

```text
[alposepolish, 23] fail: 'capture-frame linear step is dt: expected (0.0333233 - 0.0333433) actual '0''
    Total Tests:    23
    Passed Tests:   22
Failed Tests:       1
```

A temporary diagnostic prepass ran the original warm-up, capture, and next frame
for **all four combinations before the original, unmodified assertions**. The
diagnostic was removed from the delivered source after collecting the trace.

| fps | flagged | Warm-up final raw X | Warm-up final output X | Display/input X velocity | Capture linear/angular step | Next linear/angular step |
| --- | --- | --- | --- | --- | --- | --- |
| 30 | false | 0.333333 | 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| 30 | true | 0.333333 | 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| 60 | false | 0.166667 | 0.166667 | 1 / 1 | 0.0166667 / 0.0166667 | 0.0166666 / 0.0166666 |
| 60 | true | 0.166667 | 0.166667 | 1 / 1 | 0.0166667 / 0.0166667 | 0.0166666 / 0.0166666 |

Representative raw trace:

```text
T23 warmup fps=30 flagged=0 i=0 raw=0 out=0 display_v=0 input_v=0 refine=0
T23 warmup fps=30 flagged=0 i=1 raw=0.0333333 out=0 display_v=0 input_v=0 refine=1
T23 warmup fps=30 flagged=0 i=10 raw=0.333333 out=0 display_v=0 input_v=0 refine=1
T23 capture fps=30 flagged=0 linear=0 angular=0
T23 next fps=30 flagged=0 linear=0 angular=0
```

## Change

Seed the history at time `-dt/2`, then sample time zero with `dt/2`. That initial
step is 0.0166667 m at 30 fps, or 0.0083333 m at 60 fps, so it establishes the
intended 1 m/s and 1 rad/s velocities without a startup capture. All ten
subsequent warm-up intervals still use the original full-frame `dt`, as do the
seam capture and next-frame calls.

Add assertions that the pre-seam history is settled and its displayed linear
and angular velocities are 1 m/s and 1 rad/s. Keep **all four original
assertions, their expressions, expected values, and 1e-5 tolerances unchanged**.
The automatic/caller-flagged seam inputs and both tested frame rates also stay
unchanged. A production regression that holds a moving capture frame would
still fail the original capture-frame assertions.

## Validation

Only the authorized test target was built, with `MSYS_NO_PATHCONV=1`:

```powershell
cd I:/alchemy-machinima/build-Windows-vs2026-os
$env:MSYS_NO_PATHCONV = '1'
$env:TEMP = 'I:\alchemy-machinima\.codex-msbuild-tmp'
$env:TMP = $env:TEMP
cmake --build . --target INTEGRATION_TEST_alposepolish --config Release -- /m /v:minimal
.\sharedlibs\Release\INTEGRATION_TEST_alposepolish.exe
```

The initial build attempt hit `FileTracker` access denied with the default
temporary directory. Redirecting temporary files into the workspace resolved
it; the requested build command then exited 0.

Final executable output (exit 0):

```text
Unit test group_started name=alposepolish
Unit test group_completed name=alposepolish
    Total Tests:    23
    Passed Tests:   23    YAY!! \o/
```

All four test-23 iterations completed: 30/false, 30/true, 60/false, 60/true.
Each passed the new setup checks and all four original capture/next-frame
assertions. Tests 1-22 also passed in this same run.

Requested syntax/semantic checker (exit 0):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:/Users/xianw/cxs/zs_check.ps1 -Files indra/newview/tests/alposepolish_test.cpp -CachePath I:/alchemy-machinima/.tmp/alposepolish_t23_zs_flags.json
```

```text
PASS  indra\newview\tests\alposepolish_test.cpp  [PROJECT_alchemy-bin_TEST_alavatarlightresponsemodel.vcxproj]
1 TU(s), 1 PASS, 0 FAIL, 3.7s
```

The checker's newview-test mapping selects the first test project shown above;
the successful target build additionally compiles with the actual
`INTEGRATION_TEST_alposepolish` flags. The cache was directed into the workspace.
`git diff --check` passed.
