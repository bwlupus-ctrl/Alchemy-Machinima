# Sensor ISO simulation with physical post-chain order: plan rev 3 (10/25)

## REV 3 CHANGES (binding; they supersede any conflicting rev 2 text below)

Round-2 reviews from Codex, agy Argon and Kimi all returned SOUND-WITH-CHANGES. They converged on the following changes.

1. **Context gate (Kimi P0, verified in the pipeline.cpp:25945 comment).** renderFinalize also serves impostors, reflection/hero probes and HUDs. The physical order and the sensor run only when all of these hold:
   - `hdr && mRT == &mMainRT && !gCubeSnapshot`, and not a Prism, impostor or HUD render;
   - the existing no_post predicates (pipeline.cpp ~13757) pass;
   - `CineSensorEnabled` is on.

   Every other context takes the untouched legacy branch. Internally, keep "physical order" and "noise enable" as separate flags for isolation tests. The UI has one toggle, and NoiseAmount=0 still keeps gain, clip and order.
2. **Separate lazily allocated HDR scratch (Codex, Argon, Kimi).**
   - postPing/postPong default to RGB10_A2 (pipeline.cpp ~1472), which clamps to 1.
   - Stage A uses its own same-size RGBA16F ping-pong pair, allocated only when the sensor is first enabled.
   - The existing targets are never changed, so sensor OFF stays identical.
   - No shared-depth sampling feedback.
   - Keep DOF's scratch/alpha-CoC contract (pipeline.cpp ~25530) and diopter attachment 2 plus the present-depth re-warp (blit:94).
3. **Exposure placement.** Metering stays at 25938/25940. Exposure (manual or the existing adaptation) is applied exactly once, at the start of Stage A, before any optics and before electron collection. Stage C must not apply it again. S-Log3 (tonemap type 8) keeps manual-only exposure, grade bypass and sRGB cancellation (tonemapUtilF ~418, colorCorrectF ~150).
4. **Bloom, CA and flare: composite before the geometric warps** (all three reviewers). The order is:
   - exposure;
   - godrays: an additive HDR scene effect, with brightness compensated so that moving it before exposure does not change its look;
   - motion blur;
   - DOF;
   - bloom composite, CA and flare, using unwarped bloom/depth that matches the unwarped image;
   - RLVa sphere: convert its blend colour;
   - diopter/kaleidoscope;
   - fisheye;
   - optical vignette transmittance (split from colour additions to avoid double attenuation, postEffectUtilsF ~1296).
5. **HDR variants of the shaders that assume display-range input.** The originals are kept for OFF.
   - **DOF** (postDeferredF dofSample adds s.r+s.g+s.b to its weight, which causes fireflies on HDR): weights come from a bounded encoded luma, and the colour is gathered in linear.
   - **Diopter gather/composite and Kaleidoscope** (gamma 2.2 at gather:91, clamp at composite:208 and kaleido:774): linear in/out, no clamp, and hiBias/ghost terms exposure-normalised.
6. **AA: encoded detection plus linear blending** (Codex and Kimi, rejecting whole-image Karis round-tripping as non-energy-preserving).
   - Edge and luma detection run on a perceptual encoded auxiliary signal. FXAA needs perceptual luma in alpha (fxaaF:152), and SMAA edge thresholds are perceptual.
   - The blend and gather use the original linear HDR colour.
   - Remove SMAA's explicit sRGB conversions in the HDR variant (SMAA.glsl ~1402).
   - This is called "AA before sensor", not an optical low-pass filter.
7. **Overlay alignment (Codex P1).** Roto OVERLAY and actor guides drawn after the warps must follow the warped geometry. Either warp them using the diopter/fisheye uv map, or draw them before the warps on a separate overlay layer that is warped with the image and never noised. Preserve Roto Scene/Camera/Atmosphere exposure semantics (pipeline.cpp ~19280).
8. **Auxiliary feeds: noise only final presentation images** (Kimi P0, Codex P1).
   - Scene-bound textures (projector feeds, in-world screens, raw VCam rt.screen, the prismLens world signal) are never noised.
   - Only final presentation images get the sensor: the main view now, and the Prism preview in phase 2. Each gets its own stream ID and a capture-relative deterministic clock.
   - Prism/VCam presentation is phase 2.
9. **Sensor units, fully specified** (Codex and Kimi):
   - Per mode m: analog gain a_m(ISO) in DN/e⁻, pedestal B (DN), white level W (DN) and quantization to integer DN.
   - Saturation: e_sat = min(FW_m, (W−B)/a_m). Full-well and ADC clipping are modelled separately.
   - After the ADC: subtract B, divide by a_m, then rescale to linear.
   - Correct headroom statement:
     - Neutral headroom = log2(g·e_sat/(0.18·K)). It is constant only while ADC-limited with a ∝ g, and grows while full-well-limited.
     - Brightens loses 1 stop per doubling only in the ADC-limited region.
   - Presets supply calibrated (FW, σr, a, B, W) per mode, and HCG steps come only from those calibrated values.
10. **Noise statistics, fully specified:**
    - Poisson by bounded inverse-CDF for λ < 20, with a stated tail-error bound and a continuity test across λ = 20.
    - Noise is generated in camera RGB with an explicit 3×3 covariance matrix, chosen to match a demosaiced Bayer sensor.
    - Spatial correlation via a normalised kernel. Filtering is applied to Gaussian residuals only; Poisson residuals are never filtered.
    - Negative-input handling is defined.
    - LGG (colorGradeUtilF ~131) and the tone map rectify negatives, so tests measure the black mean through the ACTUAL processing chain.
11. **Hue-preserving highlights:** reconstruction uses pre-clip information, computed from the pre-clip signal in the same pass before any per-channel clipping.
12. **No-jump promise removed.** Enabling at base ISO still adds base-ISO physical noise and clipping. The readout says so.
13. **Cost.** The 0.8 ms p95 figure is an acceptance target, not an estimate.
    - Each 4K RGBA16F target is about 66 MB; motion blur is 32 taps/pixel.
    - Measure each pass separately with GPU timestamps, plus the worst-case combination (motion blur + DOF + SMAA + diopter + fisheye + sensor), under matched configurations.
    - If the target fails, report the numbers and present options to the user. No silent quality cuts.
14. **Added tests:**
    - context leakage (probes, impostors, HUD, cube);
    - projector/screen double noise;
    - no_post/legacy paths;
    - S-Log3 black-code statistics;
    - NaN, negative and HDR-extreme inputs;
    - saturated pre-clip hue;
    - resize/divisor and allocation failure;
    - overlay/depth/warp alignment;
    - frozen-feed determinism;
    - below-base ISO;
    - worst-case p95.
15. **Phasing:**
    - **P1:** context gate, scratch targets, exposure-once, the reorder with HDR variants of DOF/diopter/kaleido/SMAA/FXAA, overlay alignment, and the sensor on the main view.
    - **P2:** Prism/VCam presentation and EI mode.

Pending user decisions: the default brightness mode (Neutral proposed) and ISO vs EI (ISO proposed; EI in P2).

Rev 1 was reviewed by Codex, agy Argon and Kimi; all three returned SOUND-WITH-CHANGES. The user then chose to RESTRUCTURE the pipeline into a physical order rather than accept the split approximation. The user also confirms that Personal auto-exposure does not work today (known from the 10/08 audit). This plan does not depend on it; fixing it is a separate job and out of scope here.

## Context

- **Viewer:** Absolute Cinema, an Alchemy Second Life viewer fork for machinima, using an OpenGL deferred HDR pipeline.
- **Recording:** live output through an external capture card at 4K, targeting 60 fps on an RTX 5090. This is why ISO must be simulated in-viewer.
- **Snapshots** are not a priority.

## Current renderFinalize order (verified, pipeline.cpp)

1. Linear HDR on `mRT->screen` (RGBA16F):
   - metering: generateLuminance/generateExposure ~25938/25940;
   - on-lens filters / Soft-Edge;
   - bloom extraction;
   - froxel volumetrics and projector shafts 26045/26047;
   - projector bloom feed;
   - Roto ATMOSPHERE layer;
   - actor path guides 26072.
2. `colorCorrect(&mRT->screen, &mRT->postPingMap)` at 26078. In class1/alchemy/colorCorrectF.glsl:
   - CA/flare and bloom composite ~117–123;
   - applyExposure :138;
   - white balance :152;
   - LGG :153;
   - tone map :158;
   - LUT/gamma.
3. Display-referred, on `postPingMap`/`postPongMap`:
   - Roto OVERLAY 26099;
   - sun godray renderVolumetric 26117;
   - FXAA/SMAA 26127–26137;
   - CAS 26139;
   - (non-HDR glow combine);
   - motion blur 26155;
   - DOF 26168;
   - RLVa sphere;
   - Ultimate Diopter/Kaleidoscope 26197;
   - fisheye 26231;
   - debug visualisations;
   - final blit (gBlitWithEffectsProgram 26288: vignette, CVD, Film Grain).

There is no temporal AA, DLSS or FSR path. Prism/VCam auxiliary captures bypass renderFinalize (llprismlens.cpp ~11239).

## New order when the sensor is ON ("physical camera order")

Order: optics and scene effects, then the optical low-pass stage, then the **sensor**, then camera processing, then overlays.

### Stage A: scene and optics, linear HDR, all before the sensor

1. Existing HDR scene, volumetrics, shafts and bloom extraction, unchanged.
2. **Sun godray volumetric** (renderVolumetric) moves here. It is atmospheric, so it belongs to the scene.
3. **Motion blur** moves here, on the HDR target. Bright streaks become physically correct.
4. **DOF** moves here, on the HDR target. Bright bokeh keeps its energy, which is a visible quality gain. The DOF shaders must be audited for LDR assumptions such as clamps, sRGB weights and luma.
5. **RLVa sphere**, then **Ultimate Diopter/Kaleidoscope**, then **fisheye**: geometric and optical, kept in their current relative order. Keep the diopter_warped/attachment-2 depth re-warp plumbing.
6. **Bloom composite, CA and flare** (now inside colorCorrect) move to a linear pre-sensor point after the optics. Alternatively they stay where they are if reviewers show the order change is visually irrelevant. Bloom is lens scatter, so it is optics.
7. **Vignette:** optical falloff moves pre-sensor, as a linear multiply. Artistic colour vignette (colour/mid-colour) stays in the final blit as processing.
8. **AA (FXAA/SMAA) as the optical low-pass filter**, before the sensor. Run it in HDR through a reversible tone encode (for example Karis: `c / (1 + max(c))`, AA, then the inverse), so the edge detection and blending see perceptual values. Noise must never reach AA, because AA treats noise as edges and smears it.

### Stage B: sensor, the new pass on the linear HDR target

Gain, saturation, shot noise, read noise, ADC rails, black level and channel structure, as described in the model below. The pass also writes nothing else; it is one full-screen pass, or it is folded into the first part of a split colorCorrect.

### Stage C: camera processing, display-referred

1. The rest of colorCorrect: white balance, LGG, tone map and LUT, with no exposure and no bloom; those moved to stage A.
2. **CAS sharpening** stays here. In-camera sharpening sharpening the noise is realistic. Its default stays as it is now.
3. Roto OVERLAY.

### Stage D: overlays and debug, never noised

- Actor path guides move here; they are UI and must not get sensor noise.
- Buffer and velocity debug views.
- Final blit: artistic vignette colour, CVD and Film Grain (a stylised overlay, separate from sensor noise).

### When the sensor is OFF

The current order is preserved byte-identically.

- Both orders live in renderFinalize behind one boolean. No shader permutation changes when off; unchanged shader variants keep identity.
- Reviewers should decide whether "physical order" should also be available without sensor noise, for example for HDR bokeh alone. Default answer: no; one toggle drives both, to keep scope down.

## Sensor model (rev 1 bugs corrected)

**Per-gain-mode parameters.** Each preset defines:
- Base ISO, and the switch ISO for dual-native sensors.
- For each conversion-gain mode m (LCG/HCG):
  - input-referred read noise σr_m in electrons;
  - saturation e_sat_m = min(FW_m, ADC_white_m / gain_m);
  - ADC black pedestal and white level.
- **Brightness continuity:** total gain g = ISO / ISO_base is continuous. A mode switch changes only σr, e_sat and the rails, never brightness (Codex P1).
- **Collected light:** K (electrons per unit linear at base) is fixed and independent of mode.

**Collected light per mode:**
- Brightens mode: e = L·K, where L is the linear value after Personal Exposure.
- Neutral mode (exposure-compensated: less light, more gain, same mean): e = L·K/g.

**Noise:**
- **Shot noise:**
  - If e < 20, sample Poisson with an inverse-CDF on hashed uniforms, using a bounded loop or table.
  - Otherwise use the Gaussian approximation, N(e, √e).
- **Read noise:** add N(0, σr_m).
- **ADC:** to DN, add the black pedestal, clamp to [0, white] (both rails), subtract the pedestal. The value stays signed linear. No clamp-then-subtract after the black level, which biased blacks milky in rev 1.
- **Output:** L_out = s·g/K in both modes. In Neutral mode the mean is ≈ L.

**Headroom, with corrected physics:**
- *Brightens:* the same light plus more gain means highlights clip about 1 stop earlier relative to the scene per ISO doubling.
- *Neutral:* exposure-compensated. Highlight headroom above mid-grey stays roughly constant (e_sat/e_mid is unchanged for analog gain). Dynamic range is lost from the bottom: the noise floor rises relative to mid-grey. Rev 1's claim that Neutral shrinks headroom was wrong, so Argon's P0 is resolved by correcting the description, not by adding extra clipping. HCG mode adds a small headroom step at the switch ISO, which is realistic.

**Channel structure:**
- Noise is generated per channel in camera RGB with Bayer weighting: green has two photosites, so σG = σ/√2 relative.
- It is decomposed to luma and chroma:
  - luma noise at full resolution;
  - chroma noise on a half-resolution hash lattice with bilinear lookup, variance-normalised, giving low-frequency blotches. This avoids RGB "confetti" (all three reviewers).
- The covariance is specified and tested.

**Clipping:** per channel, then hue-preserving highlight desaturation toward neutral. This mimics camera highlight reconstruction and avoids magenta clipping (Argon P1).

**Signed values into processing:** the tone map clamps negatives. That rectification is the same thing a real camera does when it renders raw to display, and it is documented and tested against an expected black-mean curve.

**Determinism:** hash(pixel, frame counter, seed). There is an optional fixed seed per take. Auxiliary feeds get their own stream IDs. There is no fixed-pattern noise in v1.

## ISO vs Cine EI (user to confirm; proposed default ISO)

- **ISO (analog gain):** the model above.
- **EI mode, deferred to v2 unless the user wants it now:** recording stays at base ISO, and EI applies digital gain after the sensor, so read noise scales with gain and raw headroom stays at base.

## Brightness mode default (user to confirm)

Neutral is proposed: Codex and Kimi recommend it for machinima continuity, so Personal Exposure stays the brightness control. Brightens is offered as the exposure-triangle option.

The default ISO is the selected preset's base ISO, so enabling the sensor never jumps brightness or noise.

## User workflow

1. Director → Exposure page (exposure hub) → **Sensor** card → **Simulate sensor** (default off).
2. Pick ISO (third stops, 50–25600) and a Camera preset:
   - Clean full-frame: base 100.
   - Dual-native cinema: 800/3200.
   - Small/older sensor.
   - Custom.
3. Optional rows: Brightness mode (Neutral/Brightens), Noise amount (1 = physical), Chroma noise, Fixed seed.
4. A readout shows the gain mode, the headroom above mid-grey in stops, and the noise floor in stops.
5. Personal Exposure is unchanged. Film Grain remains separate.

## Settings

- `CineSensorEnabled` (false)
- `CineSensorISO`, default = the preset base
- `CineSensorPreset`
- `CineSensorBrightnessMode` (0 Neutral)
- `CineSensorNoiseAmount` (1)
- `CineSensorChroma` (1)
- `CineSensorSeedFixed` (false)
- `CineSensorSeed`
- Custom-preset parameters.

Director rows on the Exposure page must be searchable. The kit generator, page sources, functional manifest and row pin all need updating.

## Auxiliary views, v1

- VCam and Prism presentation paths (prismPreviewF, the projector feed, and in-world screens) get stage B on their own linear values before their own tone map. Each needs a stable capture ID and frame counter.
- In-world screens must not be noised twice: the VCam `rt.screen` stays raw, and the main sensor processes what the main camera sees.
- Their optics reorder is out of scope for v1 unless reviewers find it is trivial.

## ReShade

The existing `color_hdr` raw-scene contract (llreshadebridge.cpp ~414) is unchanged. ReShade still receives the pre-exposure scene. A post-sensor beauty output is v2 only if needed.

## Budget and risks

- **Cost:**
  - Sensor pass: ≤ 0.25 ms at 4K.
  - Physical reorder: motion blur, DOF and AA on 16F targets instead of LDR, plus the reversible encode and decode. Target total extra ≤ 0.8 ms p95 at 4K with the sensor on.
  - Must be measured, never assumed. Off = zero work and identity.
- **Risks:**
  - DOF, motion blur, diopter or fisheye shaders with LDR assumptions.
  - SMAA area/search textures assume perceptual input.
  - Render-target format and size changes; the diopter re-warp plumbing.
  - Roto layer semantics.
  - S-Log3 path (tonemap type 8).
  - Non-HDR path: sensor unavailable, with an explicit status.
  - Cube snapshots and probes: never noised.
  - HUD/UI: never noised.
  - Capture-card 8/10-bit output and 4:2:0 chroma subsampling, which affects how noise reads.
  - Resolution changes.

## Tests (real GL fixtures)

- **Physical order:**
  - Off identity versus the pre-change commit, full frames with every post feature combination.
  - With the sensor on, noise is absent from DOF blur, motion blur and AA output, and the noise statistics after DOF match the in-focus statistics.
  - HDR bokeh energy is conserved.
  - AA round-trip identity on noise-free edges.
- **Sensor statistics:**
  - Variance versus mean per ISO and mode, matching shot plus read noise.
  - Low-count Poisson distribution.
  - Black-mean bias curve.
  - Covariance: luma/chroma spectrum and Bayer green ratio.
  - Clip per ISO and mode, plus hue preservation.
  - Brightness continuity across the dual-ISO switch.
  - Neutral mean preservation.
  - Brightens +1 stop per doubling.
  - Determinism with a fixed seed.
- **Exclusions:** UI, guides, probes, cube snapshots and HUD carry no noise.
- **Auxiliary feeds:** isolated noise streams, no double noise on in-world screens.
- **Mutations that must fail:**
  - noise computed from output instead of collected light;
  - mode switch resets gain;
  - Gaussian at low counts;
  - clamp-then-subtract black;
  - independent RGB confetti;
  - noise before DOF or AA;
  - guides noised;
  - in-world screen double-noised;
  - auto-exposure metering post-sensor.
- **Cost:** A/B at 4K with p95 enforced, sensor off and on.

## Questions for reviewers

1. Is the physical reorder feasible in this codebase? Check stage by stage with file:line: DOF, motion blur, diopter, fisheye, SMAA/FXAA and CAS input assumptions; target formats; the diopter depth re-warp; Roto layers; S-Log3; the non-HDR path.
2. Is the corrected sensor model right? In particular the Neutral headroom physics, dual-gain parameters, low-count sampling, and signed black.
3. Should physical order be its own toggle, separate from sensor noise?
4. Should bloom composite, CA and flare move after the optics, or stay?
5. Is the reversible-encode approach right for AA in HDR, or is there a better optical-low-pass equivalent?
6. Which risks, tests or cost traps are missing?
