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

## 10. Hue-preserving clip

```
t     = smoothstep(c_knee, e_sat, max_channel(pre-clip signal))
L_out = mix(L_clipped, luminance(L_clipped) · white, t)
```

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

The random inputs come from a hash of `(pixel.xy, frame counter, seed, stream id)`. Fixed seed plus the same frame counter gives a bit-identical output.
