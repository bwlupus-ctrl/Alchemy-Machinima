# Albedo chroma explosion after the sidecar fix batch — DIAGNOSIS REQUEST

**Second opinion wanted. Assume my diagnosis is wrong and attack it.**
The user is (justifiably) angry that hours of work have not produced correct albedo in RTGI. I need
an independent read on WHY, and specifically on whether the fix batch I just landed caused this or
merely exposed a pre-existing problem.

## Build / code state

Exe built 2026-07-25 18:39 from the working tree at `90b9c8e216d` + uncommitted sidecar work +
the fix batch described in `doc/VISIBLE_DIFFUSE_SIDECAR_FIX_BATCH_BRIEF.md`
(M1-M6, S1, S5, FX1, FX2, FX4). Two Codex adversarial rounds returned 0 must-fix. Build clean.
Nothing is committed.

## RAW OBSERVATIONS, in order, all in-world

**18:47 — `SL_BridgeDebug` → "Exactness K"**
Nearly the ENTIRE frame reads GREEN (`sl_k_heat` green = K=1). A large deep-BLUE region at the top
of frame (K=0) whose lower boundary is a ragged foliage/architecture silhouette — consistent with
sky. One small ORANGE strip (0<K<1, low end) at the base of that blue region. One small deep-BLUE
arc bottom-right.
PRIOR STATE for comparison (documented, pre-fix): columns YELLOW (0<K<1), large architecture DEEP
BLUE (K=0). So K went from broken to essentially all-known.

**18:47 — `SL_BridgeDebug` → "Visible diffuse"**
Looks CORRECT. Full scene albedo, architecture reads proper base colour, greenery green, columns
gold/bronze. The avatar's wings/outfit read strongly IRIDESCENT pink/green — previously established
as genuine: that outfit really does have an iridescent base texture which scene lighting mutes.

**18:51 — `SL_GBufferProvider` → DEBUG view "Albedo iMMERSE receives"**
NEAR BLACK. Faint embossed surface detail only.
Preprocessor state at this moment: `SL_PROVIDE_ALBEDO = 0`, `SL_ALBEDO_TO_LINEAR = 1`,
`SL_PROVIDER_MODE = 0` (COEXIST).
(I attributed this to `SL_PROVIDE_ALBEDO = 0` compiling out the whole `pass ProvideAlbedo`.)

**18:54 — `SL_PROVIDE_ALBEDO = 1`, `SL_ALBEDO_TO_LINEAR = 1`, DEBUG view Off**
SEVERE rainbow / iridescent oversaturation across the whole frame. Coloured fringing on the avatar,
the architecture, and the bokeh highlights.

**18:56 — `SL_ALBEDO_TO_LINEAR = 0` (the documented-correct value), DEBUG view Off**
STILL severe rainbow oversaturation. Somewhat different but the same class of failure.
**User additionally reports: the albedo is GHOSTING IN MOTION.**

Runtime uniforms at 18:56: `SL_VISDIFF_K_THRESHOLD = 0.55`, `Motion scale = 1.00`,
`Motion flip X = TRUE`, `Motion flip Y = FALSE`.
Preprocessor: `SL_ALBEDO_FLAG_COVERAGE_GATE=1`, `SL_ALBEDO_TO_LINEAR=0`, `SL_ENABLE_DEBUG=1`,
`SL_INPUT_IS_UPSIDE_DOWN=1`, `SL_NORMAL_FLIP_Z=1`, `SL_PROVIDER_MODE=0`, `SL_PROVIDE_ALBEDO=1`,
`SL_PROVIDE_MOTION=1`.
Effect order: `iMMERSE: Launchpad` at top, `SL_GBufferProvider` directly below it, then RTGI etc.

## MY HYPOTHESES — attack these

**H-A (the one I most want checked). The fix batch did not break albedo; it ENABLED the sidecar
path for the first time, and the sidecar's true albedo is what RTGI cannot digest.**
Reasoning: `PS_ProvideAlbedo` takes the sidecar branch only when `vd.a >= threshold`. Before the fix
batch K was corrupted LOW (yellow/blue = below any sane threshold), so that branch was rarely taken
and Launchpad's incumbent estimate survived almost everywhere. Now K is correct and ~1 everywhere, so
with threshold 0.55 the sidecar supersedes essentially EVERY pixel — for the first time ever. If true,
the chroma explosion is not a regression at all; it is the first honest look at what our albedo does
to RTGI, and the fix batch is what made it visible.
**Is this reasoning sound? Is there a cheaper decisive test than the grey card below?**

**H-B. Units/calibration mismatch (flagged UNRESOLVED before this session).**
ReShade runs on the FINAL BACKBUFFER, post-exposure and post-tonemap, so the light RTGI gathers is
DISPLAY-REFERRED. Launchpad's estimate is back-derived from that same backbuffer, so it self-tracks
exposure AND is pre-distorted by the tonemap's per-channel curve. Ours is SCENE-REFERRED LINEAR true
reflectance. Tonemapping is non-linear PER CHANNEL, so the predicted mismatch is **in CHROMA, not
gain** — which is exactly the observed symptom.
Decisive test previously specified but NEVER RUN: flat neutral grey card, measure the INCUMBENT
estimate's value at SL noon vs SL midnight. Drift ⇒ units ⇒ the sidecar must publish display-referred.
**Is that test actually decisive? Design a better one if not.**

**H-C. Motion vectors wrong; ghosting is smearing the saturated albedo.**
`SL_GBufferProvider.fx:299-300`:
`if (SL_MOTION_FLIP_X) mv.x = -mv.x;` and `if (SL_MOTION_FLIP_Y) mv.y = -mv.y;  // GL NDC +Y up -> UV +Y down`
Defaults are X=true, Y=false, and the comment at :161 says "flip X for direction, keep Y ... exact
algebra, not a guess — contract v2 section 3.3". BUT the viewer's ABI publishes
`SLRESHADE_ORIENT_MOTION_NDC_Y_UP` **and** `SLRESHADE_ORIENT_DESTINATION_UV_Y_DOWN`
(`llreshadebridgeabi.h:231-234`), which is exactly the conversion FLIP_Y performs. The FX derivation
and the ABI orientation flags appear to CONTRADICT each other. The comment at :162 even predicts the
symptom: "If RTGI ghosts/smears when panning, flip an axis below."
Also relevant: **M6 in the fix batch stopped `SLRESHADE_RESET_PROJECTION_CHANGE` firing every frame.**
Before M6, temporal accumulation was being wiped every frame, so ghosting was IMPOSSIBLE. After M6 it
accumulates for the first time — so bad motion vectors would only now become visible.
**Work out the correct value of FLIP_X and FLIP_Y from the algebra. Do not guess. Show the derivation
from GL NDC (Y up, [-1,1]) to ReShade UV-space motion (Y down, [0,1], prev-minus-current or
current-minus-prev — determine which iMMERSE expects).**

## Questions

1. Which of H-A / H-B / H-C dominates the 18:56 image? They are not mutually exclusive — rank them.
2. Did anything in the fix batch actually REGRESS albedo? Run `git diff` and check. I claim no, but I
   am the author and should not be trusted on this.
3. In COEXIST mode Launchpad is loaded ABOVE us and also writes albedo. Is there a double-write,
   ordering, or feedback problem — e.g. is Launchpad re-deriving its estimate from a backbuffer that
   already contains RTGI lit with OUR albedo, producing a runaway chroma feedback loop across frames?
   **This is the possibility I am least able to rule out and most worried about.** It would explain
   both the saturation growth and the temporal component.
4. Is `SL_VISDIFF_K_THRESHOLD = 0.55` with K≈1 everywhere the right operating point, or should the
   sidecar be deliberately restricted (higher threshold, or only at pixels the G-buffer cannot answer
   — i.e. forward surfaces ONLY, which was the ORIGINAL stated purpose of the feature)?
   **Re-read the original purpose: albedo was wrong ONLY at forward-rendered pixels (alpha, water,
   fullbright). Should the sidecar be superseding albedo at deferred-opaque pixels AT ALL?** If not,
   this is a scoping error in `PS_ProvideAlbedo` and would explain everything.
5. Give me the SHORTEST sequence of in-world observations that discriminates between your ranked
   hypotheses. The user has limited patience and each trip is expensive. One trip if possible.

Answer with evidence and file:line. If you think my framing is wrong, say so directly.

## READ ALL OF THIS CODE — do not work from my summary

**Consumer side (ReShade add-on + FX):**
- `reshade-addon/shaders/SL_GBufferProvider.fx` — **the whole file.** `PS_ProvideAlbedo` is the
  function in question; also read `PS_ProvideNormals`, `PS_ProvideMotion`, `PS_ShowReceived`, the
  technique/pass list at the bottom (pass ORDER and `RenderTargetWriteMask` matter), and every
  `#define` block at the top.
- `reshade-addon/shaders/SL_Bridge.fxh` — the accessors: `SL_VisibleDiffuse`, `SL_Albedo`,
  `SL_SemValid`, `SL_UV`, `SL_SurfaceCoverage`, the sampler declarations (POINT vs LINEAR, and
  crucially whether the source is bound through an `_srgb` view so the hardware decodes on sample).
- `reshade-addon/shaders/SL_BridgeDebug.fx` — `sl_k_heat`, `PS_TailStatus`, the Visible-diffuse view.
  This is the instrument the observations above came from; if the INSTRUMENT is wrong the
  observations are worthless, so check it.
- `reshade-addon/src/sl_reshade_bridge.cpp` — `update_slot`, `map_gl_internal_format`,
  `fail_slot_closed`, and how each semantic's SRV is created/bound. **Confirm the visible_diffuse
  slot is bound through an sRGB view** — if it is bound as UNORM instead, the FX comment claiming
  "hardware already decoded" is false and every sidecar pixel is gamma-wrong, which would by itself
  produce a chroma error.

**Producer side (viewer):**
- `indra/newview/app_settings/shaders/class1/deferred/visibleDiffuseSeedF.glsl` — the classifying
  seed pass. Check the metallic split (`diffuse *= (1.0 - orm.b)`), the sky/no-answer branch, the
  windowed flag compare, and the K semantics.
- `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl` (see `visible_diffuse` writes),
  `class2/deferred/pbralphaF.glsl`, `class1/deferred/fullbrightF.glsl` — the forward writes and what
  each one puts in `.a` (K). **Check whether these are writing LINEAR or sRGB-encoded RGB**, and
  whether that is consistent with `GL_FRAMEBUFFER_SRGB` being enabled around the pass.
- `indra/newview/pipeline.cpp` — the sidecar attachment allocation (search
  `RenderVisibleDiffuseSidecar`), the seed dispatch in `renderDeferredLighting`, and the guard
  handling in `renderGeomPostDeferred`. The attachment is `GL_SRGB8_ALPHA8` and the passes run under
  `LLGLEnable(GL_FRAMEBUFFER_SRGB)` — **verify the encode/decode round trip end to end and state the
  exact transfer function at each hop.** A single missing or doubled sRGB step is a per-channel
  non-linearity, i.e. precisely a chroma error.
- `indra/newview/llreshadebridge.cpp` / `llreshadebridgeabi.h` — publication, the validity bits, the
  orientation flags, the tail layout.
- `indra/newview/lldrawpoolalpha.cpp` — the per-face indexed blend for attachment 1 and the emissive
  block.

**Docs:**
- `doc/RESHADE_BRIDGE_CONTRACT.md` (v2 — authoritative; §6.1 covers colour space, §3.3 covers motion
  algebra, §4 covers validity). If the code contradicts the contract, say which is wrong.
- `doc/VISIBLE_DIFFUSE_SYNTHESIS.md` — note its §2 corrections table has a known-WRONG entry about
  the emissive-buffer default; do not trust that row.
- `doc/VISIBLE_DIFFUSE_SIDECAR_FIX_BATCH_BRIEF.md` — what I just changed and why.

Use `git diff` for anything uncommitted. Trace the actual numbers: pick a concrete surface (say a
mid-grey stone column), and follow its albedo value hop by hop from the GLSL write, through the
GL_SRGB8_ALPHA8 attachment, through the add-on's copy and SRV binding, through `SL_VisibleDiffuse`,
through `PS_ProvideAlbedo`, into `Deferred::AlbedoTex`, and state the value at each hop. If the
number that lands is not the number that started, that is the bug.
