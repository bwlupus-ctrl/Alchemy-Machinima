# Handheld Camera Operator — Authoritative Tables

This file is the numeric source of truth for the locomotion tables implemented
by `getLocomotion()` in `indra/newview/llcameraoperator.cpp`. Values must be
reviewed by field name and value, not by aggregate-field count or position.

Locomotion replaces Motion Profile, while Operator Style remains multiplicative.
The values below are the physical target block before Style, master intensity,
reactive trims, and final per-DOF authority gains.

## Core motion and breathing

| Mode | idleAmp | panTiltFreq | rollFreq | hiContent | smoothing | energyGain | onsetGain | settleGain | drag | motionCalm | breathFreq | breathAmount |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Creep | 0.32 | 0.34 | 0.18 | 0.18 | 1.08 | 0.45 | 0.42 | 0.85 | 0.004 | 1.45 | 0.20 | 1.80 |
| Walk | 0.85 | 0.62 | 0.34 | 0.65 | 0.98 | 1.35 | 1.10 | 1.35 | 0.010 | 0.62 | 0.25 | 1.25 |
| Run | 1.55 | 1.05 | 0.72 | 1.45 | 0.82 | 2.80 | 2.35 | 2.20 | 0.017 | 0.18 | 0.38 | 1.65 |
| Drive | 0.48 | 0.78 | 1.35 | 1.70 | 0.94 | 1.10 | 0.72 | 1.55 | 0.012 | 0.78 | 0.22 | 0.55 |
| Float | 0.28 | 0.18 | 0.12 | 0.06 | 1.10 | 0.35 | 0.25 | 0.70 | 0.003 | 1.20 | 0.18 | 0.70 |
| Unsteady | 1.15 | 0.42 | 0.24 | 0.28 | 1.04 | 0.90 | 0.65 | 1.20 | 0.009 | 0.45 | 0.21 | 2.10 |

Frequencies are cycles per second. `breathAmount` is percent FOV modulation.
`drag` is radians-scale framing drag; the other reaction values are gains.

## Gait and reframing

| Mode | walkCadence | cadenceDrive | fwdBias | latBias | stepBob m | lateralStep m | stepRoll deg | gaitCouple | recomposeAmt deg | recomposeInterval s |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Creep | 0.72 | 0.12 | 0.85 | 0.12 | 0.004 | 0.003 | 0.10 | 0.65 | 0.12 | 11.0 |
| Walk | 0.83 | 0.32 | 1.00 | 0.25 | 0.017 | 0.009 | 0.38 | 1.35 | 0.28 | 7.5 |
| Run | 0.98 | 0.62 | 1.35 | 0.42 | 0.043 | 0.021 | 0.92 | 2.60 | 0.18 | 4.5 |
| Drive | 0.00 | 0.00 | 0.00 | 0.00 | 0.000 | 0.000 | 0.00 | 0.00 | 0.08 | 10.0 |
| Float | 0.00 | 0.00 | 0.00 | 0.00 | 0.000 | 0.000 | 0.00 | 0.00 | 0.12 | 14.0 |
| Unsteady | 0.45 | 0.08 | 0.25 | 0.12 | 0.006 | 0.012 | 0.75 | 0.35 | 0.42 | 5.5 |

The gait phase rate is:

`style.walkRate × walkCadence × (1 + normalizedForwardSpeed × cadenceDrive)`

At normalized forward speed 1.0 with the default Documentary Style
(`walkRate = 1.8`), the intended/measured phase rates are:

| Mode | Intended steps/s |
|---|---:|
| Creep | 1.45 |
| Walk | 1.97 (target 1.8–2.0) |
| Run | 2.86 (target 2.7–3.0) |
| Drive | 0 |
| Float | 0 |
| Unsteady | 0.87 |

## Vehicle layers

| Mode | suspHeave m | suspFreq Hz | roadBuzz m | roadFreq Hz | turnLean deg |
|---|---:|---:|---:|---:|---:|
| Creep | 0.000 | 0.00 | 0.0000 | 0.0 | 0.00 |
| Walk | 0.000 | 0.00 | 0.0000 | 0.0 | 0.00 |
| Run | 0.000 | 0.00 | 0.0000 | 0.0 | 0.00 |
| Drive | 0.018 | 1.15 | 0.0035 | 8.5 | 2.20 |
| Float | 0.004 | 0.22 | 0.0000 | 0.0 | 0.15 |
| Unsteady | 0.006 | 0.38 | 0.0000 | 0.0 | 1.40 |

Suspension and road amplitudes are multiplied by linear-speed gates. At zero
linear speed, road buzz and surge are zero. Drive retains only a separate idle
engine heave equal to `0.06 × roadBuzz` on the suspension oscillator.

## Auto locomotion defaults

Thresholds are linear camera speed normalized by
`FlycamOperatorRefLinearSpeed` (default 3 m/s). Auto entry/reset classifies the
current sample immediately: Run at or above Run Enter, Walk at or above Walk
Enter, otherwise Creep. Dwell applies only to later transitions.

| Transition | Threshold | Continuous dwell |
|---|---:|---:|
| Creep → Walk | Walk Enter = 0.24 | 0.65 s |
| Walk → Creep | Walk Exit = 0.14 | 1.10 s |
| Walk → Run | Run Enter = 0.82 | 0.55 s |
| Run → Walk | Run Exit = 0.62 | 0.90 s |

The locomotion parameter blend default is 0.85 s and the opted-in fixed
simulation rate default is 120 Hz. Legacy remains variable-step and is not
subject to either fixed-step input sampling or output interpolation.

For timestamped pose sources, the simulation clock and fixed tick times are
render-FPS-independent. The pose at each tick is nevertheless interpolated from
the render-frame polyline supplied by the caller. Results are bit-identical
across frame rates for linear path segments; on genuinely curved paths they
converge as render sampling gets denser, so only approximate cross-FPS agreement
is expected. True curved-path bit identity would require the caller to provide
its continuous path or poses sampled directly at fixed simulation times. That
caller/operator interface is follow-on work and is outside this round.
