# S-Log3 capture: colour stage (saturation matrix, highlight path-to-white, soft shoulder, camera gamut) — design brief

**Date:** 2026-10-26
**Status:** DESIGN ONLY. Nothing implemented, nothing built, **not yet adversarially reviewed**.
**Process (per `CLAUDE.md`):** Opus adversarial review of this brief, then Codex implements (`--write`,
`--prompt-file`), Opus reviews the code, loop to 0 must-fix, Claude builds once, the user tests.
Codex never builds.

> **BASE BRANCH.** `file:line` references are from `origin/fix/animesh-clone-pose-polish` at `057e558`.
> Re-anchor against any unpushed local work.

---

## 1. The problem — measured from a real recording

The user's raw, ungraded S-Log3 capture shows reds that "glow" near lights. Claude decoded the frame
(a compressed WebP screenshot, so the values are approximate) back through the S-Log3 curve:

| Measurement | Value | Meaning |
|---|---|---|
| Black level | 21–24 / 255 (expected ≈ 24) | Genuine full-range S-Log3. The encoder works. |
| Hottest red pixels (~2% of the frame) | codes **252–255 / 255**, i.e. **35–38× mid-grey** linear | **Clipping at the S-Log3 ceiling** (≈ 38.4×). It can't be recovered in post. |
| Their linear colour | about **R 35, G 0.4, B 0.4** (R/G ≈ 85) | Near-monochromatic red. Per-channel log keeps it pure red at any brightness: the "glow". |
| Scene median | ≈ 0.033 linear (≈ 2.4 stops under mid-grey) | The hot spots sit about 10 stops above the room. |

Inference, not provable from a frame: a red local light very close to the avatar (the legacy falloff
peaks hard at short range), possibly stacked with emissive neon and bloom.

Real cameras avoid this look because highlights desaturate toward white as channels saturate, and
because real materials are never this pure. The viewer's log path has neither behaviour.

---

## 2. What the code PROVES today

| Fact | Where |
|---|---|
| Tonemap type 8 is the cine log capture. Order: `max(c,0)` → EV → toe (adds `0.18·toe` flare) → per-channel OETF (S-Log3 / S-Log2 / LogC3 / Cineon) → clamp code to [0, 1] → optional legal range → `srgb_to_linear(code)` to cancel the downstream sRGB encode. | `class1/deferred/tonemapUtilF.glsl:349-416` |
| **No gamut transform**: the data is Rec.709 primaries under a log curve. The shader comment tells the user to pick *Rec.709* gamut in Resolve, not S-Gamut3. | `tonemapUtilF.glsl:358-360` |
| Type 8 returns **before** the shared saturation/contrast grade. `colorCorrectF` also bypasses its grade for type 8. So there is **no colour control at all** on the log path today. | `tonemapUtilF.glsl:461-466`; `class1/alchemy/colorCorrectF.glsl:65, 149, 165` |
| Type 8 forces manual exposure only (no auto-exposure). | `tonemapUtilF.glsl:424-425`; `pipeline.cpp:16852-16885` |
| The controls are packed into the existing `tonemap_params` vec4 (curve, range, toe, EV). **All four slots are used.** | `pipeline.cpp:12797-12808` in `LLPipeline::colorCorrect` (`:12177`) |
| Settings `AlchemyToneMapSLogCurve/Range/Toe/EV`. | `settings_alchemy.xml:1298-1341` |
| UI: Lightbox S-Log section (curve and range combos, toe and EV sliders with spinners). | `floater_lightbox_settings.xml:~770-890` |
| Reserved uniforms are registered by name in lockstep with the `llshadermgr.h` enum. `tonemap_params` and `tonemap_khronos_params` are the precedent. | `llshadermgr.cpp:1677-1680`; `llshadermgr.h:382-385` |

---

## 3. The design — a colour stage inside `encode_slog_family`

All maths is in **linear light**, after EV and before toe and OETF:

```
max(c,0) → EV → [A] saturation (Rec.709, luma-preserving, gamut-safe)
               → [B] highlight path-to-white (Rec.709, luma-preserving)
               → [C] camera gamut matrix (Rec.709 → selected gamut)
               → [D] soft shoulder (in the encoded gamut, hue-preserving)
               → toe → OETF → clamp → range → srgb_to_linear
```

**Why this order:**

- A and B are tuned in the scene's own Rec.709 space, where luma weights are defined.
- D comes last because channel clipping happens in the gamut that's actually encoded.
- Toe stays after everything: it's a neutral flare offset, and the shoulder's ceiling accounts for it.

### [A] Saturation

`Y = dot(c, vec3(0.2126, 0.7152, 0.0722))`, then `c = Y + s·(c − Y)`.

- `s = 1` is identity; range 0–1.5.
- **Luminance is exactly preserved**, so exposure and the curve's grey and brightness code values don't
  move.
- **Gamut-safe for `s > 1`:**
  - `s_max = min over channels with c_i < Y of  Y / (Y − c_i)` — the largest `s` that keeps every
    channel ≥ 0. If no channel is below Y, `s_max = ∞`.
  - `s_eff = softKnee(s, s_max)`, easing into the limit, so already-saturated colours stop boosting
    smoothly while muted colours get the full boost.
  - Below `s = 1` it's always safe; skip the limiter.
- Guard `Y ≤ 0` (black): leave `c` unchanged.

### [B] Highlight path-to-white (the fix for the measured glow)

As a colour gets bright, blend it toward its own luminance. Hot reds go orange-pink, then white, like a
real sensor.

- **Key on the hottest channel, not on Y.** A pure red at R = 35 has Y ≈ 7.4, which looks "moderate",
  while its red channel is about to clip. `m = max(c.r, c.g, c.b)`.
- `t = clamp((log2(m / 0.18) − knee_ev) / range_ev, 0, 1)`, then `w = strength · t²·(3 − 2t)`
  (smoothstep).
- `c = mix(c, vec3(Y), w)`. **Luminance is preserved** (mixing toward Y keeps Y).
- **Defaults when enabled:** `knee_ev = +3` (8× mid-grey), `range_ev = 3` (fully neutral by 64× unless
  strength < 1), `strength = 1`.
- **Worked check against the measured frame:** R = 35 gives `log2(35/0.18) ≈ 7.6`. So t = 1 and the pixel
  goes fully neutral at strength 1. At strength 0.6 it becomes about (18.6, 4.8, 4.8): R/G falls from about
  85 to about 3.9, with Y unchanged at about 7.76.

### [C] Camera gamut (optional)

A 3×3 from Rec.709 to the selected gamut. **D65 white for all, no chromatic adaptation needed.**

| Gamut | Use with | Primaries (xy) — **verify against the vendor whitepaper** |
|---|---|---|
| Rec.709 (default) | any curve | R 0.640, 0.330 · G 0.300, 0.600 · B 0.150, 0.060 |
| S-Gamut3.Cine | S-Log3 / S-Log2 | R 0.766, 0.275 · G 0.225, 0.800 · B 0.089, −0.087 |
| S-Gamut3 | S-Log3 / S-Log2 | R 0.730, 0.280 · G 0.140, 0.855 · B 0.100, −0.050 |
| ARRI Wide Gamut 3 | LogC3 | R 0.6840, 0.3130 · G 0.2210, 0.8480 · B 0.0861, −0.1020 |

- Cineon has no defined gamut: force Rec.709 when Cineon is selected.
- **Build the matrix on the CPU from the primaries, with a pure function** (standard RGB→XYZ from
  primaries and white, then `M = XYZ→target · Rec709→XYZ`). **Never hand-type matrix coefficients.**
  The primaries above are from memory; Codex must check them against Sony's S-Gamut3 / S-Gamut3.Cine
  technical summary and ARRI's LogC3 / AWG3 whitepaper before use.
- Rec.709 sits inside all three wide gamuts, so non-negative input stays non-negative. The conversion
  is lossless; it adds no colour (the renderer works in Rec.709).
- **Cost to state:** a wider gamut packs the same colours into fewer code values. An 8-bit recording may
  band slightly more in saturated gradients; 10-bit capture is unaffected.

### [D] Soft shoulder

The goal is to stop channels hitting the code ceiling, without hue shifts.

- `C` = the curve's linear ceiling (inverse OETF of code 1.0), computed on the CPU per curve (S-Log3 ≈
  38.4). `C_eff = C − 0.18·toe`, because toe is added afterwards.
- `m = max(c.r, c.g, c.b)` (in the encoded gamut). With start `S = C_eff · start_frac` (default 0.25,
  i.e. 2 stops below the ceiling):
  - if `m > S`: `m' = S + (C_eff − S)·(1 − exp(−(m − S)/(C_eff − S)))` and `c *= m'/m`.
- **Ratio-preserving scale**, so hue holds. It's continuous with slope 1 at `S`, and `m'` approaches
  `C_eff` but never reaches it.
- **Interaction with B:** B removes most of the chroma that makes D necessary; D is the safety net for
  anything still hot (neutral highlights included).

---

## 4. Plumbing

- **New reserved uniforms**, appended **in lockstep** to the `llshadermgr.h` enum and the
  `mReservedUniforms.push_back` list (precedent `llshadermgr.cpp:1677-1680`; the startup assert
  catches a mismatch):
  - `slog_color_params` (vec4): `.x` = s, `.y` = knee_ev, `.z` = range_ev, `.w` = strength;
  - `slog_shoulder_params` (vec4): `.x` = S, `.y` = C_eff, `.z` = shoulder enable (0/1), `.w` = stage
    enable (0/1);
  - `slog_gamut_mat` (mat3), identity for Rec.709.
- Declare them in `tonemapUtilF.glsl` beside `tonemap_params` (`:144`). Upload them in
  `LLPipeline::colorCorrect` case 8 (`pipeline.cpp:12797`), **only when the stage is enabled**.
- **Stage enable** = `s ≠ 1 || strength > 0 || shoulder on || gamut ≠ Rec.709`. When it's off, the
  shader takes the exact existing path.

### Settings (`settings_alchemy.xml`, beside `:1298-1341`)

| Key | Type | Default | Range |
|---|---|---|---|
| `AlchemyToneMapSLogSaturation` | F32 | 1.0 | 0–1.5 |
| `AlchemyToneMapSLogHighlightDesat` | F32 (strength) | 0.0 | 0–1 |
| `AlchemyToneMapSLogHighlightKnee` | F32 (EV over mid-grey) | 3.0 | 0–8 |
| `AlchemyToneMapSLogHighlightRange` | F32 (EV) | 3.0 | 0.5–6 |
| `AlchemyToneMapSLogShoulder` | Boolean | false | — |
| `AlchemyToneMapSLogShoulderStart` | F32 (fraction of ceiling) | 0.25 | 0.05–0.9 |
| `AlchemyToneMapSLogGamut` | S32 | 0 | 0 Rec.709, 1 S-Gamut3.Cine, 2 S-Gamut3, 3 ARRI WG3 |
| `AlchemyToneMapSLogZebra` | Boolean | false | — (§6) |

### UI (Lightbox S-Log section, below the EV row)

- Saturation slider and spinner; Highlight desat strength, knee and range; a Shoulder check with its
  start slider; a Gamut combo (entries filtered by curve: S-Gamut options with S-Log, AWG3 with LogC3,
  Rec.709 only with Cineon); a Zebra check.
- A ↺ reset per control.
- **Reflow:** shift everything below and raise the containing panel heights. Prove there's no overlap or
  clipping by rect arithmetic. The panel lives in the Lightbox floater; check whether any other host
  embeds it.

### Docs

Update the `tonemapUtilF.glsl:356-361` comment and `doc/MACHINIMA_USER_GUIDE.md` with the Resolve
settings per gamut:

- **Rec.709:** input gamut Rec.709, gamma = the curve (today's instructions).
- **S-Gamut3.Cine / S-Gamut3 / AWG3:** select that gamut with its matching gamma in the CST. Camera LUTs
  now apply correctly.
- **Saturation ≠ 1 or path-to-white on:** the footage is no longer neutral log, so camera LUTs will look
  correspondingly different. That's expected.

---

## 5. Hard rules

1. **Off = unchanged, proven by image diff.** A runtime uniform branch can't prove byte identity (the
   compiler may schedule the untouched path differently; round-1 lesson from the Relight brief). Required
   evidence: an **off-vs-baseline image diff** of a fixed scene with tonemap type 8, all four curves, both
   ranges. Or use a `#define` permutation, added **after** `clearPermutations()`.
2. **Other tonemappers untouched.** The stage lives only inside `encode_slog_family`.
3. **Determinism:** pure per-pixel maths. No temporal state, no noise.
4. **No negative or NaN output:** guard `Y ≤ 0`, `m ≤ 0` and `log2(0)`; clamp the final code as today.
5. **Off-limits:** the other tonemappers, `colorCorrectF` grading, exposure logic, the ReShade bridge.
   Never build.

---

## 6. Instrument — zebra (a standard camera tool)

`AlchemyToneMapSLogZebra` draws diagonal stripes over pixels where **any channel, before [D], is ≥ 90%
of `C_eff`** (pixels that are clipping or would clip without the shoulder). This tells the user,
before recording, exactly where the measured problem is.

- It **renders into the backbuffer**, so in log mode **it would be recorded**. It must therefore switch
  off automatically when the UI is hidden or Capture mode is armed. Log `[SLogColor] zebra suppressed
  (capture)` when that happens.
- Log a verdict line on any setting change: `[SLogColor] curve S-Log3, ceiling 38.42, C_eff …, shoulder
  start …, gamut …, stage ON/OFF`.

---

## 7. Tests

**TUT, pure header `alslogcolormath.h`** (literals, each must fail if the thing it tests is broken):

- **Saturation:** `s = 1` is identity exactly; luminance is preserved for random colours and `s` in
  [0, 1.5]; `s = 0` gives `(Y, Y, Y)`; with the limiter, no channel goes below 0 for `s` up to 1.5 on
  fully saturated primaries.
- **Path-to-white:** strength 0 is identity; luminance is preserved; with the defaults, `(35, 0.4, 0.4)`
  goes fully neutral at strength 1; at strength 0.6, R/G for the same pixel lands within 3.7–4.1 (expected ≈ 3.88). Keying
  on max-channel: a pure red with Y below the knee but R above it still desaturates.
- **Gamut:** the matrix from primaries maps D65 white `(1, 1, 1)` → `(1, 1, 1)` within 1e-5; Rec.709
  primaries map to non-negative triples; for Rec.709 → Rec.709 the matrix is identity; the inverse
  round-trips within 1e-5.
- **Shoulder:** continuous and slope 1 at `S`; output is strictly below `C_eff` for any finite input;
  hue (channel ratios) is preserved exactly; the ceiling per curve equals the inverse OETF of code 1.0
  (S-Log3 ≈ 38.42).

**In-world, outcomes stated in advance** — reproduce the user's mirror/bathroom shot (red light near an
avatar):

1. **Everything off:** the image diff against baseline is identical. Codes on the hot reds are still
   252–255.
2. **Zebra on:** stripes appear on the left avatar's torso, legs and neon strip (the measured hot
   regions). Zebra off when Capture is armed: no stripes in the recording.
3. **Shoulder on:** no channel reads above about 250 / 255 on those regions; hue unchanged.
4. **Path-to-white 0.6:** the hottest reds read orange-pink; decoded R/G drops from about 85 to about 4;
   brightness of grey and skin areas unchanged.
5. **Saturation 0.85:** overall chroma down; luminance (decoded Y) unchanged on a grey card.
6. **Gamut S-Gamut3.Cine:** in Resolve, CST *S-Gamut3.Cine / S-Log3 → Rec.709* matches the Rec.709-gamut
   version of the same frame (CST *Rec.709 / S-Log3 → Rec.709*) within grading tolerance on a colour
   chart.

---

## 8. Open decisions (defaults chosen)

- **D1 — Path-to-white default:** strength 0 (off), so existing footage is unchanged until enabled.
  Suggested starting point for the user's red-light scenes: strength 0.6, knee +3 EV.
- **D2 — Shoulder default:** off. Recommended **on** for all recording once verified.
- **D3 — Per-hue control** (Sony-style colour depth): deferred. Saturation plus path-to-white cover the
  measured case.
- **D4 — Zebra threshold:** 90% of `C_eff`, fixed. A user-set threshold is deferred.
