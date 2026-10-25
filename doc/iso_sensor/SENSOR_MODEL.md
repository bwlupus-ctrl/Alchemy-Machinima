# Sensor model: equations (EMVA 1288 linear model)

These equations are binding for WP-A, alongside PLAN.md rev 3. Symbols are listed per gain mode m: LCG below `ISO_switch`, HCG at or above it. Single-gain presets have only LCG.

## 1. Light to electrons

```
μ_e = K · L / g_N
```

- `L` is the linear value after Personal Exposure.
- `g_N = 1` in Brightens mode.
- `g_N = ISO / ISO_base` in Neutral mode (the default).

## 2. Shot noise

```
n_e ~ Poisson(μ_e)
```

- For `μ_e < 20`, use bounded inverse-CDF sampling on a hashed uniform. The loop is capped and the tail-error bound is documented.
- Otherwise use `n_e = μ_e + sqrt(μ_e) · z`, with `z` drawn by Box–Muller from hashed uniforms.
- The two regimes must be continuous across `λ = 20`.
- The lower CDF is used through uniform 0.9999; above it, sum the survival
  probability downwards from `min(64, ceil(λ + 10 sqrt(λ) + 6))`
  (start at 3 for `λ < 1e-4`, at 6 for `λ < 0.01`, avoiding underflow).
  This avoids subtracting a small tail from one in float32. The omitted tail
  is below `2e-14` for `0 < λ < 20`; the uniform grid spacing is `2^-23`.

## 3. Dark current

This is deferred to v2. It would add `μ_d` here, and is not implemented now.

## 4. Full well

```
n_e' = min(n_e, FW_m)
```

## 5. Amplifier and read noise

```
a_m(ISO) = a_base,m · ISO / ISO_base
signal_DN = a_m · (n_e' + σ_r,m · z_r)
```

- Total gain is continuous across `ISO_switch`.
- The mode switch changes only `FW`, `σ_r`, `a_base` and `B`/`W`.

## 6. ADC

```
DN = round( clamp( signal_DN + B, 0, W ) )
e_sat = min(FW_m, (W − B) / a_m)
```

The rounding contributes quantisation noise of `1/12 DN²`.

## 7. Back to linear

```
ŝ_e   = (DN − B) / a_m
L_out = ŝ_e · g / K          g = ISO / ISO_base (total gain), in BOTH modes
```

The result is signed. It is never clamped here.

Consequences:
- **Neutral:** `μ_e = K·L/g`, so `E[L_out] = L`. Brightness is unchanged.
- **Brightens:** `μ_e = K·L`, so `E[L_out] = g·L`, which is +1 stop per ISO doubling.

`g_N` appears ONLY in the collection step (section 1). Correction 10/25: an earlier text used `g_N` here, which wrongly kept Brightens flat.

## 8. Photon transfer curve (the test oracle)

```
σ²_total(e⁻) = μ_e + σ²_r,m + 1/(12·a_m²)
SNR = μ_e / σ_total
```

Dynamic range in stops is `log2(e_sat / σ_r,m)`.

Headroom above OUTPUT mid-grey:

| Mode | Headroom (stops) |
|---|---|
| Neutral | `log2(g · e_sat / (0.18·K))` |
| Brightens | `log2(e_sat / (0.18·K))` |

In Brightens the scene's mid-grey is at `0.18·K` electrons and its output is `0.18·g`. The output clip is at `e_sat·g/K`, so the gain cancels. Headroom shrinks one stop per doubling only through `e_sat` itself, in the ADC-limited region where `e_sat = (W − B)/a_m` and `a_m ∝ ISO`.

Both forms are the same quantity: `log2(L_out_clip / L_out_mid)`. Tests use this definition.

Correction 10/25: an earlier text divided by `g` a second time; Codex WP-A found it.

## 9. Colour structure

```
n_rgb ≈ C · z
C · Cᵀ = Σ_rgb = M · diag(σ²_R, σ²_G/2, σ²_B) · Mᵀ
```

- `M` is the preset's camera-to-working colour matrix.
- The per-channel `σ²` values come from section 8.
- Spatial correlation: decompose into luma and chroma. Luma stays at full resolution. Chroma comes from a half-resolution hash lattice with bilinear interpolation, divided by `sqrt(Σ w_i²)` so its variance is preserved.
- Spatial filtering applies to the Gaussian part only. Poisson residuals are never filtered.

For NoiseAmount `A` and ChromaAmount `c`, let `D = M^-1 C [axis,u,v]`,
where `axis = normalize(C^T luma)` and `[axis,u,v]` is orthonormal. The
Gaussian camera covariance is `S = A² D diag(1,c²,c²) D^T`. Apply this
separately to shot and read noise; add the unfiltered low-count Poisson
variance and the ADC variance separately. Section 8 is the physical `A=c=1`
oracle, away from clipping rails.

Define `V_G = D_G0² + c²(D_G1²+D_G2²)`. Reconstruct green sites from
`G1 = μ_G + A (averageResidual + sqrt(V_G) z_difference)` and
`G2 = μ_G + A (averageResidual - sqrt(V_G) z_difference)`, independently
for Gaussian shot and read. Their marginal variance is `2 A² V_G` and
their covariance is zero for every `A,c`. Low-count shot uses the two
independent Poisson samples directly, regardless of `c`. Clip each well and
digitise each site before averaging. This corrects the previously unspecified
non-unit-chroma decomposition, which left the difference variance unchanged.

The half-resolution lattice uses `position = 0.5 pixel + 0.25 + 0.5 bits`,
where the two origin bits come from the avalanched capture clock, seed and
stream. This gives symmetric sub-cell weights with a random pixel origin.
Variance remains normalised;
averaging over frames removes fixed two-pixel correlation modulation.

## 10. Hue-preserving clip

```
q_i   = smoothstep(c_knee · e_sat, e_sat, pre_clip_i)
t     = (q_R + q_G + q_B) / 3
L_out = mix(L_clipped, luminance(L_clipped) · white, t)
```

`c_knee` is the dimensionless knee fraction (0.8). `pre_clip_G` is the
maximum of the two individual green sites, so a single saturated site is
retained. The mix is continuous and monotonic in every pre-clip channel,
and reaches full neutral only when all three camera channels saturate.
This replaces the earlier peak-only equation, which reached neutral on
the first saturated channel. Luminance uses `(0.2126,0.7152,0.0722)`.

## 11. Fixed-pattern noise

DSNU, PRNU and banding are deferred to v2 and not implemented.

## 12. Presets

Each preset stores, per mode: `FW`, `σ_r`, `a_base`, `B`, `W`, plus `ISO_base`, `ISO_switch`, `M` and `H` (headroom stops). Then:

```
K = e_sat,base / (0.18 · 2^H)
```

Initial presets use illustrative published-class values, labelled as illustrative:

- **Clean full-frame:** base ISO 100, FW 50000, σ_r 3.0 e⁻, 14-bit, B 512, H 3.5.
- **Dual-native cinema:** 800 LCG / 3200 HCG.
  - LCG: FW 40000, σ_r 6.
  - HCG: FW 10000, σ_r 1.5.
- **Small / older sensor:** base 200, FW 15000, σ_r 5, 12-bit.

## Determinism

The random inputs come from an ordered hash of `(pixel.x, pixel.y, frame low,
frame high, seed, stream id, domain)`. Spatial and capture/domain sub-tuples
avalanche every component before a final avalanche combines them. Fixed seed
plus the same frame counter gives a bit-identical output.

`ALSensorModel::pack` caches the ordered capture-context hash in
`Uniforms::randomContext.x` using the same uint32 operations as the shader.
Use `setClock` when changing a packed clock so this cache stays in sync;
the GPU layout is 224 bytes. Pixel and domain hashing remain in GLSL.
This moves only integer work: the random streams and floating-point equations
are unchanged. Quarter-cell lattice phases have `sum(w_i^2) = 25/64`, so
their normaliser is exactly `5/8`; coordinates that lose that phase during
float conversion retain the general normaliser. Upper-tail Poisson lanes
are excluded from the lower-CDF exit condition because the survival sampler
overwrites those lanes.
