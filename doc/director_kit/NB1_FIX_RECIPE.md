# NB1 fix recipe (verified by the reviewer's harness)

The harness is `C:\Users\xianw\cxs\alkitmath_harness.ps1`. At HEAD it FAILS with 257,211 pitch cases. With the recipe below applied to a scratch copy, it PASSES with 0 failures.

`baseFromDisplayPitch`:
- Take ONLY the two principal solutions.
- `wrap180` each one.
- Keep the ones with |q| ≤ 85.
- Pick the one nearest `q0` (current base + orbit).
- Then clamp.

Allowing ±90 instead still leaves 5 failures when OffsetZ == radius.

Harness tolerances:
- Where |OffsetZ| ≥ radius, the base pitch is multi-valued, so it's checked on the displayed angle.
- Side-view yaw gets 0.5° near the camera axis and the poles, where acos is imprecise.
