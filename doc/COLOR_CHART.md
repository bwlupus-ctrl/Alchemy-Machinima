# Colour chart

Open **Personal Lighting > Image**, below the log capture controls, or **Director v2 > Look > Image > Colour chart**. Show enables a reference for this session. Mode selects Unlit or Lit. Size is 0.10?0.60 of frame height; Anchor selects Centre or Lower third. Placement Camera follows the final viewer camera (including VCam and flycam). **Pin here** freezes the current position, rotation and size in the set; selecting Pinned also pins the current camera placement. Toggle colour chart is an ordinary Preferences > Controls > Machinima action, unbound by default. There is one hotkey; pinning uses the button.

Unlit is a float HDR reference. Its 18% grey is exactly the float representation of 0.18, never an 8-bit texture approximation. Lit is a single nonselectable client-only matte legacy box with a generated sRGB texture, no fullbright, glow, shine, bump or material specular. Lit measures the set lighting; its grey texture necessarily uses 118/255. The black border and 16-pixel gutters surround 128-pixel patches. Rows 1?4 contain the Classic 24 patches; row 5 contains white, grey and black spanning two columns each.

Pinned **Unlit draws without a depth test, over occluders**. Lit uses normal scene depth. Normal region crossings preserve the global pin and recreate the Lit object in the current region. After teleport, a pin outside the destination region, or a pin more than 512 m from the camera, returns to Camera with one notification. Nothing is sent to the simulator. Hiding immediately destroys the Lit prim and releases chart textures, targets and optional shaders. Disconnect/logout clears the pin and nonpersistent placement before world teardown; final application cleanup repeats this idempotently. Lit camera placement publishes its drawable immediately without world-object damping; pinning freezes that displayed transform. Showing and placement are nonpersistent; mode, size and anchor are preferences. No chart state is added to scenes, setups, clone captures, presets or exports.

## Exactness policy

**Reads through Sensor purity: set purity to 1.00 when you want the chart to match the published values exactly in Magic Bullet.** There is no purity bypass. The scene and chart use the same camera transfer. At purity 0.90 the reference records the purity-reduced colour, not the original published patch.

| Stage | Unlit behaviour |
|---|---|
| Manual RenderExposure and log EV | Applied exactly once, including sensor Stage A compensation |
| Auto exposure | Chart never drives metering; type 8 ignores adaptation, other operators use the live exposure map |
| Sensor purity, camera gamut, toe, OETF and Full/Video range | Applied through production camera shaders |
| Sensor well/noise | Sensor path passes through Stage B; disable CineSensorNoiseEnabled for a clean read and use neutral sensor/well settings |
| Bloom/glow, DOF, blur, CA, flare, fisheye, diopter, vignette, AA, haze/fog, SSR, weather | Bypassed by chart injection after optics |
| CAS and Roto/guide overlays | Sensor chart can receive these post-encode composites; sample flat patch centres |
| Final Present artistic vignette/grain/CVD/preview | Chart rectangle restored from its pre-Present encoded source |
| External Present effects | Affect the recording; disable them for a clean reference or use the chart to measure their contribution |

The legacy pipeline encodes before DOF and lens warps. To keep the chart exact without changing scene rendering, a separate RGBA32F chart atlas is exposed and encoded using the production Exposure and Camera stages, then only the chart rectangle is drawn after final Present. Sensor mode injects into its existing exposed float source immediately before Stage B, then restores the chart rectangle from the final encoded source after Present. Purity and sensor noise remain intact. The chart shader is loaded lazily while shown. The legacy atlas uses five private production encoder programs (Exposure and the four Camera permutations), without loading or unloading the physical sensor suite. Shader or atlas failure is latched until explicit hide/show or a shader reload. Hidden has one cached Show test at each of the two pipeline sites and in idle; the developer-readback test is nested inside the same Show gate. No chart GL work, allocation, per-frame setting lookup or prim exists on the hidden path.

Environment only (AlchemyEnvEnvironmentOnly) scales scene lighting rather than masking RENDER_TYPE_VOLUME: Unlit is independent of that lighting, while Lit responds normally. The feature requires the normal main-view HDR/post-processing path; it skips cubemaps, Prism lens/preview, impostors, HUDs, non-main targets and no-post rendering.

## Record and grade

1. Set log capture type 8, **S-Log3 / Sony S-Gamut3 / Full**, toe 0, EV 0, RenderExposure 1.0 and Sensor purity **1.00**. Disable sensor noise and external effects for the clean reference. Keep the sensor neutral if enabled.
2. Show **Unlit**, choose a useful size and record **1?2 seconds at the start of a set**. Sample patch centres; avoid edges, labels, guides and overlay ink. Hide it for the take, or pin it to the chosen spot when it needs to stay visible.
3. In Vegas/Magic Bullet, choose the matching **S-Log3 / S-Gamut3** camera input / ACES 1.2 Sony IDT and Full levels. Apply one input transform. The values below describe recording codes before the input transform, not the final display look.
4. If deliberately recording at purity 0.90, an IDT alone returns the reduced colour. In linear Rec.709 after undoing the camera gamut, use `Y = .2126 R + .7152 G + .0722 B`, then `c = Y + (c_reduced - Y) / purity`. Undo any toe before inverse gamut and undo EV/exposure last. Purity 1.00 avoids this custom correction.

## Published patch and expected-code table

Lead-verified X-Rite ColorChecker Classic published sRGB set, pre-November-2014 production. The generator derives IEC 61966-2-1 linear Rec.709 from these bytes. W/G/K are added references; the Unlit grey override is 0.18. Defaults: RenderExposure **1.0**, log EV 0, toe 0, S-Log3/S-Gamut3 Full. Both purity columns are shown because 0.90 is the sensor-purity default. Codes are normalized 0?1; multiply by 1023 for ten-bit codes or round(code * 255) for eight-bit codes.

| # | Patch | sRGB8 | Linear Rec.709 | Code at purity .90 | Code at purity 1.00 |
|---|---|---|---|---|---|
| 1 | Dark skin | 115 82 68 | 0.171441, 0.084376, 0.057805 | 0.375226, 0.338299, 0.310294 | 0.377631, 0.336813, 0.304716 |
| 2 | Light skin | 194 150 130 | 0.539479, 0.304987, 0.223228 | 0.501779, 0.470662, 0.445443 | 0.503849, 0.469429, 0.440657 |
| 3 | Blue sky | 98 122 157 | 0.122139, 0.194618, 0.337164 | 0.403862, 0.424276, 0.466376 | 0.402431, 0.425161, 0.470789 |
| 4 | Foliage | 87 108 67 | 0.095307, 0.149960, 0.056128 | 0.361902, 0.379678, 0.319459 | 0.359974, 0.379875, 0.310568 |
| 5 | Blue flower | 133 128 177 | 0.234551, 0.215861, 0.439657 | 0.443553, 0.442847, 0.494110 | 0.444024, 0.443242, 0.498820 |
| 6 | Bluish green | 103 189 170 | 0.135633, 0.508881, 0.401978 | 0.465352, 0.511668, 0.498367 | 0.460468, 0.512722, 0.497991 |
| 7 | Orange | 214 126 44 | 0.672443, 0.208637, 0.025187 | 0.505881, 0.435948, 0.331270 | 0.509784, 0.432531, 0.299035 |
| 8 | Purplish blue | 80 91 166 | 0.080220, 0.104616, 0.381326 | 0.365290, 0.380767, 0.472525 | 0.364993, 0.382107, 0.479808 |
| 9 | Moderate red | 193 90 99 | 0.533276, 0.102242, 0.124772 | 0.476136, 0.387191, 0.382720 | 0.480974, 0.382980, 0.377803 |
| 10 | Purple | 94 60 108 | 0.111932, 0.045186, 0.149960 | 0.339262, 0.305388, 0.377171 | 0.342067, 0.304840, 0.382622 |
| 11 | Yellow green | 157 188 64 | 0.337164, 0.502886, 0.051269 | 0.488846, 0.504844, 0.383977 | 0.486891, 0.504821, 0.356457 |
| 12 | Orange yellow | 224 163 46 | 0.745404, 0.366253, 0.027321 | 0.527989, 0.484639, 0.363543 | 0.530547, 0.482551, 0.327831 |
| 13 | Blue | 56 61 150 | 0.039546, 0.046665, 0.304987 | 0.308766, 0.323472, 0.445496 | 0.309115, 0.325293, 0.454000 |
| 14 | Green | 70 148 73 | 0.061246, 0.296138, 0.066626 | 0.392162, 0.444351, 0.355062 | 0.385967, 0.445209, 0.340882 |
| 15 | Red | 175 54 60 | 0.428690, 0.036889, 0.045186 | 0.443466, 0.319075, 0.299149 | 0.449331, 0.311821, 0.287757 |
| 16 | Yellow | 231 199 31 | 0.799103, 0.571125, 0.013702 | 0.547499, 0.525596, 0.384724 | 0.548684, 0.524349, 0.341282 |
| 17 | Magenta | 187 86 149 | 0.496933, 0.093059, 0.300544 | 0.473820, 0.394403, 0.455003 | 0.478565, 0.391387, 0.458415 |
| 18 | Cyan | 8 133 161 | 0.002428, 0.234551, 0.356400 | 0.370327, 0.435569, 0.472224 | 0.363423, 0.437318, 0.476861 |
| 19 | White 9.5 | 243 243 242 | 0.896269, 0.896269, 0.887923 | 0.583905, 0.583875, 0.583187 | 0.583904, 0.583870, 0.583105 |
| 20 | Neutral 8 | 200 200 200 | 0.577580, 0.577580, 0.577580 | 0.535892, 0.535892, 0.535892 | 0.535892, 0.535892, 0.535892 |
| 21 | Neutral 6.5 | 160 160 160 | 0.351533, 0.351533, 0.351533 | 0.481976, 0.481976, 0.481976 | 0.481976, 0.481976, 0.481976 |
| 22 | Neutral 5 | 122 122 121 | 0.194618, 0.194618, 0.191202 | 0.418620, 0.418565, 0.417313 | 0.418616, 0.418556, 0.417163 |
| 23 | Neutral 3.5 | 85 85 85 | 0.090842, 0.090842, 0.090842 | 0.340232, 0.340232, 0.340232 | 0.340232, 0.340232, 0.340232 |
| 24 | Black 2 | 52 52 52 | 0.034340, 0.034340, 0.034340 | 0.249015, 0.249015, 0.249015 | 0.249015, 0.249015, 0.249015 |
| W | Linear white | 255 255 255 | 1.000000, 1.000000, 1.000000 | 0.596027, 0.596027, 0.596027 | 0.596027, 0.596027, 0.596027 |
| G | 18% grey | 118 118 118 | 0.180000, 0.180000, 0.180000 | 0.410557, 0.410557, 0.410557 | 0.410557, 0.410557, 0.410557 |
| K | Black | 0 0 0 | 0.000000, 0.000000, 0.000000 | 0.092864, 0.092864, 0.092864 | 0.092864, 0.092864, 0.092864 |

Grey = 420/1023 = **0.410557**, white 1.0 = **0.596027**, black = 95/1023 = **0.092864**. At RenderExposure 2.0, grey = **0.484546**; EV +2 multiplies scene light by 4 before encoding. Video range maps Full codes with `(code * 876 + 64) / 1023`.

The production-shader comparison tolerance is **0.25/1023**. An eight-bit backbuffer additionally rounds by up to 0.5/255; ten-bit rounds by up to 0.5/1023. These transport quantisation bounds do not relax the float-shader gate. The published bytes do not claim to reproduce every physical card, illuminant, print batch or camera spectrum. Lit values depend on illumination and are not expected to match the Unlit code table.

The plan's TE quantisation estimate was incorrect: sRGB 118 decodes to **0.1811642442**, giving S-Log3 grey **0.411235363**, error **0.000678178** (0.693776 ten-bit codes). It fails both 0.25/1023 and 0.5/1023. The requested stricter tolerance is retained.

## Developer checks

```
$env:PYTHONUTF8='1'
python scripts/code_tools/colorchart_gen.py --check
python scripts/code_tools/colorchart_check.py
python scripts/code_tools/colorchart_check.py --gpu
```

The GPU runner refuses while AlchemyTest.exe is running. It uses unchanged production camera shaders, independent Sony/primary-derived CPU expectations, the installed Vegas ACES 1.2 OCIO IDT through slog_gamut_ocio_check.run, CAT02 and Bradford comparison, inverse purity, and the A7-style transport fixture. The complete production finalize methods also run in the existing instrumented native dispatch harness against commit 398f7592e05, including hidden debug-readback and disconnected cases. Native /WX fixtures compile the production optional loader, failure latches, session cleanup and sit senders against message spies; compiling negative mutations must fail their behavioral checks. Both chart sampling modes (atlas and screen restore) have GPU coverage with projective placement, a nonzero viewport origin and zero copy tolerance. Every negative control must produce a numerical or event-stream FAIL; a compile failure does not count as rejection. All mutations stay in memory or generated test fixtures.

**PENDING-INWORLD:** enable developer-only, session-only AlchemyColorChartDebugReadback. Every 60 presented chart frames it reads back each visible patch centre after Present (before swap), logging PASS/FAIL/INCONCLUSIVE, path, actual/expected codes, differences, exposure and EV. The independent double expected model covers S-Log3 Rec.709/S-Gamut3, live purity, toe and range. Unsupported curves/gamuts, enabled sensor noise, nonfinite reads and offscreen centres are INCONCLUSIVE. Offscreen/snapshot FBO presentation is reported INCONCLUSIVE before any window backbuffer read. PASS/FAIL uses the float-shader bound plus half an output code at the actual eight/ten-bit backbuffer depth. Test both Sensor and Legacy at EV 0/+2 and RenderExposure 1.0/2.0. Check Camera, both anchors, pinning behind an occluder, Lit orientation/material/light response, hide/show, seamless crossing, teleport recovery and environment-only lighting. No viewer is launched by the automated checks.

Developer failure control: session-only AlchemyColorChartForceShaderFailure = 1 forces the chart shader unavailable; 2 forces the private legacy encoder unavailable; 0 permits normal loading. Set the control, then hide/show the chart or reload shaders. A failed attempt logs once and subsequent frames continue presenting the scene without repeated compilation. Reset to 0 and hide/show or reload to recover.
