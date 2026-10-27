# Log capture gamut

Personal Lighting > Image and Director > Look > Image > S-Log3 capture expose
**Log gamut** beside Log curve and Range. `AlchemyToneMapSLogGamut` is persisted:
0 Rec.709 (legacy), 1 Sony S-Gamut3 (default), 2 Sony S-Gamut3.Cine,
3 ARRI Wide Gamut 3. Changing it updates uniforms without rebuilding shaders.

For new S-Log3 footage choose the matching camera input, for example **Sony
S-Log3 / S-Gamut3** in Resolve CST, **Sony - S-Log3 - S-Gamut3** in Magic Bullet
Looks OCIO-ACES, or the matching Vegas ACES media colour space. Apply one input
transform only: a media ACES input plus a second camera CST/IDT double-transforms
the footage. Match Full/Legal data levels too. S-Log3 also pairs with
S-Gamut3.Cine; LogC3 pairs with AWG3. For Cineon or S-Log2 select Rec.709 unless
your tool expects another gamut. Older footage encoded before this feature
uses Rec.709 primaries and still needs a Rec.709 input gamut.

Sensor purity (`AlchemyToneMapSLogPurity`, persisted F32) defaults to **0.90**,
range 0.60?1.00. Its slider sits beside Log gamut in both UIs. Shared Saturation
is disabled in type 8 and points to Sensor purity; other tonemappers keep their
existing behavior. Both settings update uniforms without shader reloads.

The encoder applies purity to nonnegative linear Rec.709 after EV, then the
gamut matrix, optional linear toe and OETF. All three matrices preserve D65 white. Option 0 skips the
new arithmetic. Matrices derive from [Sony's technical summary, p.8](https://www.sony.jp/ls-camera/knowledge/pdf/TechnicalSummary_for_S-Gamut3Cine_S-Gamut3_S-Log3_V1_00.pdf)
and [ARRI's LogC VFX document, p.10](https://www.arri.com/resource/blob/31918/66f56e6abb6e5b6553929edf9aa7483e/2017-03-alexa-logc-curve-in-vfx-data.pdf).
If toe is nonzero, undo its `0.18 * toe` offset in camera-linear space before
the inverse matrix; then undo purity in Rec.709 as described below and undo EV.
A standard camera input alone does not undo custom toe/purity. No highlight
limiter, path-to-white or hue rotation is added.

For `Y = dot(c, (0.2126, 0.7152, 0.0722))`, purity is `c' = Y + s*(c-Y)`.
Luminance is invariant, so its inverse is `c = Y + (c'-Y)/s`, using the
luminance of `c'`. At 0.90, pure red becomes **(0.92126, 0.02126, 0.02126)**;
pure magenta becomes **(0.92848, 0.02848, 0.92848)**. Some supplied examples
had inconsistent green/blue numbers; these values follow the approved formula.
The shader branches around the operator at 1.00 and for equal-channel neutrals.
The real sensor well/noise simulation runs earlier in `pipeline.cpp`: Stage B
`renderSensor` collects/clips, then Stage C `colorCorrect(...Camera)` invokes
the encoder. Purity therefore runs after that simulation, immediately after
log EV. No well/clip simulation occurs inside this encoder. Godray forward/inverse
probes and the auxiliary Roto-overlay camera projection can evaluate the transfer
before collection, but the beauty buffer stays linear there; the final beauty
encode still follows Stage B. These probes do not add another purity operation
to the final beauty path.

## Consumer audit

Audited `applyToneMap`, `encode_slog_family`, `tonemap_params`, `tonemap_type`,
log inversions, false-colour and headroom consumers in viewer C++/GLSL.

| Consumer / anchor | Domain and handling |
|---|---|
| `tonemapUtilF.glsl::encode_slog_family` | Sole log encoder. Purity after EV, matrix after purity, before toe/OETF; downstream sRGB cancellation is unchanged. |
| `colorCorrectF.glsl::cameraTransfer` and Legacy TONEMAP programs | Shared encoder and type-8 grade bypass. `pipeline.cpp::colorCorrect` uploads gamut on every bind. |
| Sensor Camera `[2..5]` and Godrays `[6..9]` (`llviewershadermgr.cpp::loadSensorShaders`) | Same upload path. Sensor Exposure/Optics return before the encoder. The sensor collects/clips working Rec.709 first, so purity and gamut conversion run once in Camera afterward. |
| `colorCorrectF.glsl` `AL_SENSOR_GODRAYS` | Sole reconstruction of linear light from encoded camera output. Wide gamut or purity below 1 uses `decode_slog_family`: undo range/OETF/toe, inverse matrix, inverse purity, inverse EV. Grade bypass makes this analytic for all four curves. Apply the decoded increment to the original scene so energy beyond a clipped code is retained; in the unclipped domain this equals the exact inverse. Rec.709 with purity 1 retains its old search. Unreachable negative-channel targets are bounded to nonnegative light. |
| `class1`/`class3/deferred/volumetricLightF.glsl` | Sensor variant emits a display-unit increment consumed by the inverse above. Legacy variant adds an artistic increment directly to delivered output codes; it does not claim to reconstruct linear Rec.709. No inverse is applied to that output overlay. |
| Prism preview, projector feed and projector look TONEMAP programs | `prismPreviewF` / `prismProjectorFeedF` call the shared encoder for camera sources. `uploadPrismPreviewColorCorrectUniforms` covers preview (`llprismpreview.cpp`) and both pipeline projector bind paths. SDR media skips the encoder. Projector look effects are deliberately baked output effects; turn them off for clean log. |
| VCam / `prismLensF` / `prismSignalF` retained beauty | Scene-linear auxiliary capture; no log inverse. Main presentation encodes once. Preview/feed selects camera transfer separately. |
| `postDeferredTonemap.glsl` | Contains an older encoder call, but no shader recipe references this entry-point filename. Its feature utility uses the same uniform if reactivated; current executable consumers are the programs above. |
| `blitWithEffectsF` / `postEffectUtilsF` | Finish, CVD and debug false-colour consume delivered output. False-colour thresholds are output-code luma, not scene stops or a log inverse. With log selected these are camera codes. Keep preview/CVD/artistic finish off for clean capture. No new scene-linear readout is implied. |
| Sensor model headroom, light-rig headroom, exposure/ink helpers, diopter exposure weighting | Computed before log from scene/sensor light or scalar manual exposure. A white-preserving matrix does not change those units. No inverse required. There is no zebra log decoder in this checkout. |
| Snapshots | Normal post path calls `colorCorrect` and gets the uniform. No-post/raw snapshots intentionally bypass all camera encoding. No file writer decodes log codes. |
| Settings / resets / presets / scene save | Persist S32 exactly like the existing SLog settings. Kit history and per-row reset include the bound setting; Image Reset tone explicitly includes it. Image has `pre: none` (no preset provider), and existing SLog controls have no scene-file serialization, so neither a preset provider nor a scene category is added. |

Post-encode AA, spatial warps, grain, display effects and file copies preserve
their existing domains; none reconstructs scene-linear Rec.709 from log. A
spatial interpolation of log samples remains an interpolation of log samples.

## Verification

Run `$env:PYTHONUTF8='1'; python scripts/code_tools/slog_gamut_check.py --gpu`.
The CPU purity check failed before implementation (missing purity uniform).
The final CPU and production-GPU checks pass:

- Primary-coordinate matrices (1e-6), white/row sums, exact neutral operator,
  luminance preservation, nonnegativity, pure-red formula, inverse ordering,
  settings/UI/synonyms, uniform/enum lockstep and no-reload contracts.
- Direct GPU probe of the production purity helper: neutral/white byte identity,
  luminance within the float32 forward-error bound, correct red at 0.90 and
  complete byte identity at 1.00.
- Delivered encode and actual godray inverse across all four gamuts, all four
  curves for inverse consumers, both ranges, grade variants, and purity
  0.60/0.90/1.00. Clipped-highlight energy remains intact. Prism preview,
  projector feed and projector look also pass at each gamut/purity.
- Purity 1 is byte-identical to round one in 184 cases (96 encoder, 64 godray,
  24 Prism); Rec.709/purity 1 matches
  original HEAD in 24 cases. Frozen round-one source fixtures and SHA256
  provenance live in `scripts/code_tools/slog_gamut_fixtures/round1/`.
- Per PURITY_BRIEF item 6a, only the newly authored delivered-output tests use
  absolute tolerance `0.5/1023 = 0.0004887586`. No preexisting test tolerance
  changed. The separate HEAD control measures the same inherited sRGB precision
  floor (maximum 0.0002002716 in the original fixtures). Production sRGB code
  remains unchanged. Matrix/operator/inverse tests keep their tighter bounds.
- Generator check/selftest, page-source/manifest checks, kit lint, curation and
  draw lint pass; existing warnings remain. The honest mockup pin is 2309 rows.
- Native `xui_layout_check.py --changed HEAD --json` reports two findings.
  Read-only HEAD comparison of every touched XUI file, including original C++
  visibility sources and identical widget defaults, proves **zero NEW findings**.
  Both are the existing Rendering/Shadows Lightbox scroll-document overflows.
- `scripts/code_tools/director_header_check.py` fails identically on HEAD and
  worktree. Its generated `headers.cpp` is byte-identical (SHA256
  `5ca12c42ffe005d61edb8640a67ec600b86838a256476bd545675bd873cbf52f`).
  The same three fixture errors occur: C3861 `isMasterWidget` not found,
  C2065 `mMaster` undeclared, C2039 `ALKitPage::isMasterWidget` absent. No branch
  change caused this fixture defect. Full compiler logs and the HEAD loader are
  in `.tmp/slog_gamut/purity/`.
- Legacy schema/parity findings from round one remain separately documented;
  no production gate was weakened or given a new exemption.

## Shader emulator evidence

Run `$env:PYTHONUTF8='1'; python scripts/code_tools/slog_gamut_emulator.py`.
Both GPU tools re-check that `AlchemyTest.exe` is absent before creating a
context. All GPU runs completed after that check; there is no pending GPU run.

Outputs in `.tmp/slog_gamut/emulator/`: four labelled grade charts, four raw-log
charts, four vectorscopes, `comparison.png`, `decoded_values.csv` (108 rows),
`summary.json` with renderer/shader provenance, and reference-search evidence.
Every requested patch is rendered by the actual production shader at EV 0/+2/+4
for Rec.709/S-Gamut3 crossed with purity 1.00/0.90.

The offline grade always assumes S-Gamut3: published S-Log3 inverse, S-Gamut3
linear to Rec.709, clip, IEC sRGB encode. This is an **ACES-free reference**,
not a Magic Bullet look or a highlight DRT. Round three's broader offline search
found exact ACES 1.2 configs in installed Vegas folders and a working bundled
PyOpenColorIO runtime. The separate IDT comparison below uses that reference;
no RRT/ODT approximation was used.

Legacy misread pure red at EV 0 becomes approximately
**(1.878002, -0.176814, -0.026202)** before clipping. Matching S-Gamut3 recovers
the intended linear colour; purity 0.90 pulls the primaries inward and grey/white
codes remain identical. Code error against the matching reference is at most
6.25e-6. At EV +4, the inherited sRGB precision corresponds to up to 0.0007701
absolute linear error at white 16 (about 0.0048%); the CSV retains these values.
Logarithmic decode scales absolute errors with scene brightness. Saturation
`(max-min)/max` is reported separately on decoded linear, clipped linear,
display sRGB and raw log values. Negative misdecoded channels may exceed 100%
before clipping.

In-world UI validation and lead-owned `zs_check` remain pending. No viewer
build, commit, push, HEAD change or additional worktree was performed.

## Round three: external code anchors

Run `$env:PYTHONUTF8='1'; python scripts/code_tools/slog_gamut_external_check.py --gpu`.
It returns **1**, preserving the requested A8 Bradford failure; all other anchors
pass. Evidence is in `.tmp/slog_gamut/tests3/results.json`, `gpu.log`,
`aces_search.json` and `ocio_comparison.json`. No shader decoder is used for
correctness anchors. A6 explicitly labels its decoder round trip as consistency
and additionally tests independent encoder codes.

| Anchor | Result and evidence |
| --- | --- |
| A1 orientation | PASS: GPU delivered red codes `(0.533697, 0.323799, 0.214025)` within 0.001 of the lead's values. In-memory transposition produces green `0.479256` and fails. |
| A2 neutrals | PASS: full `(0.596032, 0.584458, 0.410563)`, legal `(0.572948, 0.563036, 0.414129)` for white 1, white 0.90, grey 0.18. Byte-identical across four gamuts and all 41 purity UI steps plus an off-step value. Neutral bypass logic covers the continuous interval. |
| A3 order | PASS: delivered red codes at purity 0.90, inverted with Sony's published CPU equation, give `(0.530731, 0.090531, 0.041379)` within 1e-4 of the supplied anchor. Moving purity after the matrix fails the code predicate and matches the supplied swapped-order values. |
| A4 uploads | PASS within the native/GL fixture: 25 programs, including Legacy four variants, Camera[2..5], Godrays[6..9], Prism preview/feed/projector-look grade/gamma variants, and dormant `postDeferredTonemap`. Compiles the production C++ upload helper, unchanged colorCorrect upload prefix, and full Prism upload method. The shader-uniform seam forwards to real GL; `glGetUniformiv==3` and AWG3 red codes pass in each. Omitting the production helper call fails both readback and code checks in every program, then the real upload restores the result. Godrays needs an explicitly selectable encoder probe because its normal main returns reconstructed light. Full viewer dispatch remains pending. |
| A5 ceiling/negatives | PASS: direct code tap gives exactly 1 for red 50 with legacy gamut/purity 1; G/B match Sony codes. A colored ray retains scene red exactly 50. With SG3 and black scene plus `(0,0.3,0.3)` display ray, red clamps exactly to zero and G/B match independent Sony-inverse/primary-derived matrix values within 3e-5. Red 50 does **not** reach the ceiling after SG3 conversion, so that assertion is deliberately scoped to legacy controls. |
| A6 consistency | PASS: chromatic patch, EV +4, toe 0.3, purity 0.6, legal, four gamuts. Maximum decoded-linear error 3.424e-5, below 4.888e-4; independent encoder code references also pass. |
| A7 quantisation | PASS: 64 samples from code 0.090 to 0.120, production Camera through RGBA8/16F/32F intermediates and production final blit into RGBA8. Every chain is monotonic with maximum one-code step and byte-identical to HEAD. Incorrectly storing the precompensated linear signal in RGBA8 creates six-code steps and fails. The external float-code predicate remains 0.001; the output quantisation test uses the requested step predicate rather than assuming CPU rounding at bin boundaries. OS/recorder transport is pending. |
| A8 external IDT | **FAIL at the required Bradford 0.001 gate**: GPU codes through installed ACES 1.2 Sony IDT differ by at most **0.002983607** from independent Rec.709 -> AP0 with Bradford D65 -> ACES white. The installed Sony IDT matrix uses **CAT02** instead: independent CAT02 comparison error is **0.000051379**. The gate and production matrices remain unchanged. Additional independent Sony XYZ check passes at 1e-5 (maximum 0.000001305). |
| A9 pairing guidance | PASS: both Lightbox and generated Director UI explicitly say Cineon/S-Log2 should use Rec.709 unless the grading tool expects otherwise, and give standard S-Log3 pairings. |

The external IDT is read in place from
`C:/Program Files/VEGAS/VEGAS Pro 22.0/OpenColorIO/configs/aces_1.2/config.ocio`.
Eight exact ACES 1.2 configs were found among 54 local OCIO configs. Current
Python has no PyOpenColorIO; an isolated tiny test executable loads the existing
BorisFX Sapphire Python 3.7 DLL and its **PyOpenColorIO 2.4.2** module. No package
was installed or downloaded, and no application installation was modified.
The config SHA256, GPU input codes, actual/expected AP0 values and compile/runtime
logs are retained. CAT02 is diagnostic evidence, not a substitute passing gate.

Emulator charts now include 90% white and overlay lead-verified Sony codes beside
measurements. Other references derive from independently typed primaries and the
published OETF, not production matrix literals. CSV includes `external_code_*`.
`ocio_reference_comparison.png` shows the failed Bradford reference beside the
actual IDT and CAT02 explanation. Vectorscopes use clipped, display-encoded sRGB,
never log codes. The raw-log chart labels its separate code saturation explicitly.

Negative shader mutations exist only in memory; native omission mutations affect
generated test fixtures only. Production shader SHA256 is checked unchanged
afterward. CPU contracts and `git diff --check` pass. No additional production
C++ files changed in round three. In-world UI/dispatch, recorder full/limited
levels, and lead-owned `zs_check` remain pending. The cumulative production C++
list remains `llshadermgr.cpp`, `alfloaterlightbox.cpp`, `llfloaterdirector.cpp`,
and `pipeline.cpp`.
