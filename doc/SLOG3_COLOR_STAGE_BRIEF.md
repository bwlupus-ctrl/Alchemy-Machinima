# S-Log3 capture: colour stage (saturation matrix, highlight path-to-white, soft shoulder, camera gamut) — design brief

**Date:** 2026-10-26
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Review:** round 1 (independent Opus) found 2 must-fix, 5 should-fix and 4 nits. All were applied in
rev 2. Claude re-verified the shoulder failure and the replacement numerically in float32. The rev-2
edits aren't re-reviewed; the code review must attack §3 [B] and [D] and §4 first. The disposition is
in the Appendix.
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
  - `s_eff = 1 + (s_max − 1)·(1 − exp(−(s − 1)/(s_max − 1)))` for `s > 1`. It equals 1 at `s = 1` with
    slope 1 (continuous with the `s ≤ 1` branch), never exceeds `s_max`, and tends to `s` as
    `s_max → ∞`. If `s_max − 1 < 1e-4`, use `s_eff = s_max`. In float32 it can reach `s_max` exactly;
    that gives a channel of exactly 0, never negative. Already-saturated colours stop boosting smoothly
    while muted colours get the full boost.
  - Below `s = 1` it's always safe; skip the limiter.
- Guard `Y ≤ 0` (black): leave `c` unchanged.

### [B] Highlight path-to-white (the fix for the measured glow)

As a colour gets bright, blend it toward its own luminance. Hot reds go orange-pink, then white, like a
real sensor.

- **Key on the hottest channel of the ENCODED gamut, not on Y.** A pure red at R = 35 has Y ≈ 7.4, which
  looks "moderate", while its red channel is about to clip. Clipping happens in the gamut that's
  encoded, so `m = max(M·c)`, where `M` is the [C] matrix (identity for Rec.709). With a wide gamut, a
  Rec.709 key would fire about 0.6 stops early (pure red (35, 0, 0) is (22.6, 3.06, 1.29) in
  S-Gamut3.Cine). The mix itself stays in Rec.709.
- `t = clamp((log2(m / 0.18) − knee_ev) / range_ev, 0, 1)`, then `w = strength · t²·(3 − 2t)`
  (smoothstep).
- **Neutral target (a stated decision):** `N = Y + target·(m_709 − Y)`, where `m_709` is the Rec.709 max
  channel, then `c = mix(c, vec3(N), w)`.
  - `target = 0` is constant luminance: a hot red becomes a **grey** at its own Y. That's not what a
    sensor does, and it makes the max channel **non-monotonic** at strength > 0.6 (a brighter input
    gives a dimmer output; reviewer-proven, confirmed numerically: a 0.34% drop at strength 1).
  - `target = 1` is a neutral at the max-channel level, like a sensor clipping to white: luminance
    rises.
  - **Default `target = 0.5`**: halfway. Claude's float32 sweep (hues: red, near-red, blue, orange,
    green, violet; 0.05–200 linear; strengths 0.6, 0.8, 1.0) found the max channel **monotonic for
    every target ≥ 0.5 at every strength tested.** `target` is clamped to [0.5, 1].
- No channel goes negative (a convex mix of non-negative values). [D] handles any brightness the
  target adds.
- **Defaults when enabled:** `knee_ev = +3` (8× mid-grey), `range_ev = 3`, `target = 0.5`,
  `strength = 1`.
- **Worked check against the measured frame** (Rec.709 gamut; `log2(35/0.18) ≈ 7.6`, so t = 1):
  - strength 0.6: (35, 0.4, 0.4) → about **(26.8, 13.0, 13.0)**. R/G falls from about 85 to about 2.1; Y
    rises from 7.76 to 15.9;
  - strength 1: fully neutral at about 21.4.

### [C] Camera gamut (optional)

A 3×3 from Rec.709 to the selected gamut. **D65 white for all, no chromatic adaptation needed.**

| Gamut | Use with | Primaries (xy) — **verify against the vendor whitepaper** |
|---|---|---|
| Rec.709 (default) | any curve | R 0.640, 0.330 · G 0.300, 0.600 · B 0.150, 0.060 |
| S-Gamut3.Cine | S-Log3 / S-Log2 | R 0.766, 0.275 · G 0.225, 0.800 · B 0.089, −0.087 |
| S-Gamut3 | S-Log3 / S-Log2 | R 0.730, 0.280 · G 0.140, 0.855 · B 0.100, −0.050 |
| ARRI Wide Gamut 3 | LogC3 | R 0.6840, 0.3130 · G 0.2210, 0.8480 · B 0.0861, −0.1020 |

- Cineon has no defined gamut: force Rec.709 when Cineon is selected.
- Offer S-Gamut3.Cine **only with S-Log3**. No Sony camera pairs S-Log2 with S-Gamut3.Cine, so S-Log2
  gets Rec.709 or S-Gamut3.
- **Build the matrix on the CPU from the primaries, with a pure function** (standard RGB→XYZ from
  primaries and white, then `M = XYZ→target · Rec709→XYZ`). **Never hand-type matrix coefficients.**
  The primaries above are from memory; Codex must check them against Sony's S-Gamut3 / S-Gamut3.Cine
  technical summary and ARRI's LogC3 / AWG3 whitepaper before use.
- Rec.709 sits inside all three wide gamuts, so non-negative input stays non-negative. The conversion
  is lossless; it adds no colour (the renderer works in Rec.709).
- **Cost to state:** a wider gamut packs the same colours into fewer code values. An 8-bit recording may
  band slightly more in saturated gradients. The post target is `GL_RGB10_A2` (`pipeline.cpp:1341`), but
  the window backbuffer and recorder bit depth aren't established, so whether a given capture is 10-bit
  is **inference**.

### [D] Soft shoulder

The goal is to stop channels hitting the code ceiling, without hue shifts.

- **Ceilings** (inverse OETF of code 1.0, from the shader's own formulas; reviewer-computed): S-Log3
  **38.421**, S-Log2 **13.758**, LogC3 EI800 **55.080**, Cineon **13.522**. The legal range maps code
  after the clamp, so it doesn't change these.
- **Target, not ceiling:** `T = OETF⁻¹(0.98) − 0.18·toe` (S-Log3: 32.085 at toe 0). Aiming at the
  ceiling itself fails on the GPU: an exponential approach underflows in float32 and lands **exactly on
  the ceiling** (code 255) for inputs around 200× and above. The measured 35–38× is a clip value, so
  the true scene values can be far higher.
- `m = max(c.r, c.g, c.b)` in the encoded gamut. Start `S = T · start_frac` (default 0.25).
- If `m > S`: `x = (m − S)/(T − S)`, `m' = S + (T − S)·x/(1 + x)`, and `c *= m'/m`.
  - **Rational, not exponential:** slope 1 at `S`, asymptote `T`, and a heavy tail that spreads
    highlights across codes instead of packing everything above about 50× into the top few codes.
  - Float32 check (Claude, S-Log3, start 0.25):

    | Input m | Code /255 |
    |---|---|
    | 35 | 237.6 |
    | 100 | 245.1 |
    | 500 | 248.9 |
    | 1e4 | 249.8 |
    | 1e8 | 249.9 |

    It never reaches 255.
- **Ratio-preserving scale**, so hue holds. Since `m' ≤ T < C_eff`, no channel reaches the code ceiling.
- **Interaction with B:** B removes most of the chroma that makes D necessary; D is the safety net for
  anything still hot (neutral highlights included).

---

## 4. Plumbing

- **New reserved uniforms**, appended **in lockstep** to the `llshadermgr.h` enum and the
  `mReservedUniforms.push_back` list (precedent `llshadermgr.cpp:1675-1682`). **The lockstep check at
  `llshadermgr.cpp:2146` is an `llassert`, which compiles out in Release** (`llerror.h:50-76`), so
  verify the enum and the name list **by counting** (`CLAUDE.md` rule 6), or build RelWithDebInfo
  once:
  - `slog_color_params` (vec4): `.x` = **s − 1**, `.y` = knee_ev, `.z` = range_ev, `.w` = strength;
  - `slog_shoulder_params` (vec4): `.x` = S, `.y` = T (the target, §3 [D]), `.z` = shoulder enable (0/1),
    `.w` = stage enable (0/1);
  - `slog_highlight_target` (float): the path-to-white target, clamped to [0.5, 1]. It's a fourth
    reserved uniform, because the vec4s are full;
  - `slog_gamut_mat` (mat3), identity for Rec.709.
- Declare them in `tonemapUtilF.glsl` beside `tonemap_params` (`:144`). Upload **all three, every
  frame**, in `LLPipeline::colorCorrect` case 8 (`pipeline.cpp:12797`), with the enable flag carrying
  the off state.
  - `colorCorrect` picks one of four persistent program objects (`pipeline.cpp:12209-12216`), and each
    keeps its uniforms between frames. "Upload only when enabled" would leave a stale `enable = 1` in a
    program after the user turns the stage off, or after `RenderColorGrade` switches variants.
- **Zero means identity:** encode saturation as `s − 1` in `.x`, so a zero-initialised or partly
  uploaded block is identity, never `s = 0` (monochrome). Strength 0, shoulder off and enable 0 are also
  identity at zero.
- **Stage enable** = `s ≠ 1 || strength > 0 || shoulder on || gamut ≠ Rec.709`. When it's off, the
  shader takes the exact existing path.
- `TONEMAP_PARAMS` is uploaded only in `colorCorrect`, and `applyToneMap` is called only from
  `colorCorrectF` (water links `tonemapUtilF` but never calls the encoder), so there are no other
  upload sites (reviewer-verified).
- `uniformMatrix3fv(U32 index, …)` exists for the mat3 (`llglslshader.h:262`).

### Settings (`settings_alchemy.xml`, beside `:1298-1341`)

| Key | Type | Default | Range |
|---|---|---|---|
| `AlchemyToneMapSLogSaturation` | F32 | 1.0 | 0–1.5 |
| `AlchemyToneMapSLogHighlightDesat` | F32 (strength) | 0.0 | 0–1 |
| `AlchemyToneMapSLogHighlightKnee` | F32 (EV over mid-grey) | 3.0 | 0–8 |
| `AlchemyToneMapSLogHighlightRange` | F32 (EV) | 3.0 | 0.5–6 |
| `AlchemyToneMapSLogHighlightTarget` | F32 | 0.5 | 0.5–1 (0.5 = halfway, 1 = sensor white) |
| `AlchemyToneMapSLogShoulder` | Boolean | false | — |
| `AlchemyToneMapSLogShoulderStart` | F32 (fraction of ceiling) | 0.25 | 0.05–0.9 |
| `AlchemyToneMapSLogGamut` | S32 | 0 | 0 Rec.709, 1 S-Gamut3.Cine, 2 S-Gamut3, 3 ARRI WG3 |
| `AlchemyToneMapSLogZebra` | Boolean, **non-persistent** (starts off every launch) | false | — (§6) |

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

- It **renders into the backbuffer**, so in log mode **it would be recorded**. Rules:
  - **Draw it in the final blit, not in `colorCorrect`.** `colorCorrect` runs before SMAA/FXAA/CAS
    (`pipeline.cpp:21927`), which would soften the stripes. Feed the blit a pre-[D] "would clip" mask
    from `colorCorrect`.
  - **Main view only.** `renderFinalize` runs once per view (`altron.cpp:729`), and the GLTF material
    preview calls `colorCorrect` with tonemapping on (`llgltfmaterialpreviewmgr.cpp:563`). So snapshots,
    VCam/Prism feeds, auxiliary views and previews never get stripes.
  - **Suppression signal:** use Capture mode's **raw armed state**, not
    `LLViewerTexture::isCaptureModeActive()`. That accessor returns false under critical memory even when
    Capture is pinned (`llviewertexture.cpp:513-519`), which would bring the stripes back mid-take. Also
    suppress when the UI is hidden. The auto-arm signals can each be switched off (commit `0eaf483`), so
    don't rely on them alone.
  - **Non-persistent setting:** it starts off at every launch.
  - Screen recorders often capture with the UI visible (inference), so the floater shows a red
    "ZEBRA ON — will be recorded" warning while it's on.
  - Log `[SLogColor] zebra suppressed (capture)` when it's suppressed.
- Log a verdict line on any setting change: `[SLogColor] curve S-Log3, ceiling 38.42, C_eff …, shoulder
  start …, gamut …, stage ON/OFF`.

---

## 7. Tests

**TUT, pure header `alslogcolormath.h`** (literals, each must fail if the thing it tests is broken):

- **Saturation:** `s = 1` is identity exactly; luminance is preserved for random colours and `s` in
  [0, 1.5]; `s = 0` gives `(Y, Y, Y)`; with the limiter, no channel goes below 0 for `s` up to 1.5 on
  fully saturated primaries.
- **Saturation knee:** `s_eff(1) = 1`; slope 1 at 1; `s_eff ≤ s_max` in float32 up to `s = 1.5`.
- **Path-to-white:**
  - strength 0 is identity;
  - with `target = 0.5`, defaults and the Rec.709 gamut, `(35, 0.4, 0.4)` at strength 0.6 gives
    ≈ (26.83, 12.99, 12.99), within 1e-2. At strength 1 it gives a neutral ≈ 21.38;
  - **monotonic:** the max channel is non-decreasing along geometric ramps (0.05–200) for the six hues
    above, at target 0.5 and strengths 0.6, 0.8, 1.0 (float32). The test must FAIL with `target = 0`
    and strength 1;
  - the key uses the encoded-gamut max: with S-Gamut3.Cine, a pure red's knee fires later than with
    Rec.709.
- **Gamut:** the matrix from primaries maps D65 white `(1, 1, 1)` → `(1, 1, 1)` within 1e-5; Rec.709
  primaries map to non-negative triples; for Rec.709 → Rec.709 the matrix is identity; the inverse
  round-trips within 1e-5.
- **Shoulder:**
  - continuous and slope 1 at `S`;
  - **in float32**, output is ≤ `T` and the encoded code is ≤ 0.98 for m up to 1e8 (S-Log3: code
    ≤ 249.9/255);
  - hue (channel ratios) is preserved exactly;
  - ceilings match: S-Log3 38.421, S-Log2 13.758, LogC3 55.080, Cineon 13.522 (±1e-3).

**In-world, outcomes stated in advance** — reproduce the user's mirror/bathroom shot (red light near an
avatar):

1. **Everything off:** the image diff against baseline is identical. Codes on the hot reds are still
   252–255.
2. **Zebra on:** stripes appear on the left avatar's torso, legs and neon strip (the measured hot
   regions). Zebra off when Capture is armed: no stripes in the recording.
3. **Shoulder on:** no channel reads above 250 / 255 anywhere (it's mathematically capped at code
   0.98); hue unchanged.
4. **Path-to-white 0.6 (target 0.5):** the hottest reds read pink-white rather than neon red; decoded R/G
   on them drops from about 85 to about 2; grey and skin areas below the knee are unchanged.
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
- **D5 — Path-to-white target:** 0.5 (halfway between constant luminance and sensor white). 1.0 is
  available for a harder "clips to white" camera look.

---

## Appendix — review round 1 disposition (applied in rev 2)

| # | Finding | Disposition |
|---|---|---|
| M1 | "Upload only when enabled" leaves a stale enable in persistent program objects | Upload every frame; zero-means-identity encoding (§4) |
| M2 | The exponential shoulder hits the ceiling exactly in float32 (code 255 from about 200×) | Rational shoulder aimed at code 0.98; float32 test to 1e8 (§3 [D], §7) |
| S1 | Constant-luminance path-to-white is non-monotonic at strength > 0.6 and goes grey, not white | Neutral target `Y + target·(m − Y)`, default 0.5, clamped ≥ 0.5; monotonic sweep test (§3 [B]) |
| S2 | Key on the Rec.709 max, while clipping happens in the encoded gamut | Key on `max(M·c)` (§3 [B]) |
| S3 | `softKnee` undefined | Defined, with properties and tests (§3 [A]) |
| S4 | Zebra suppression leaky (memory-masked accessor, per-view `renderFinalize`, GLTF preview) | Raw armed state, main view only, non-persistent, on-screen warning (§6) |
| S5 | The lockstep `llassert` compiles out in Release | Verify by counting (§4) |
| N1–N4 | Zebra before AA; 10-bit unproven; S-Log2 + S-Gamut3.Cine unusual; line refs | Final-blit zebra; labelled inference; S-Gamut3.Cine only with S-Log3; refs fixed |
