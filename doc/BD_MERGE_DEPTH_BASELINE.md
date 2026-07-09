# [BDMerge A0.2] Depth-Buffer Baseline Characterization

Reference for how the Alchemy base populates and reads the scene depth buffer,
captured before any depth-touching merge item (A2.x, A3.2) lands. Line numbers
are as of commit `d1e60e092a` (2026-07-09); they will drift, the structure
won't.

## 1. Allocation

- Main scene depth is a **`GL_DEPTH_COMPONENT24` depth *texture*** (no
  stencil), point-filtered, attached to `mRT->deferredScreen`
  (`pipeline.cpp:878-885`, `allocateScreenBufferInternal`). Format resolution:
  `llrendertarget.h:92` (default `DEPTH_FMT_24`) →
  `llrendertarget.cpp:94-109`.
- `mRT->deferredScreen.shareDepthBuffer(mRT->screen)` (`pipeline.cpp:885`) —
  the HDR `screen` target **shares the same depth attachment**; there is one
  scene depth buffer, not two.
- Resolution derives from window size × `RenderResolutionDivisor`
  (`pipeline.cpp:859-865`) and `RenderResolutionMultiplier`
  (`pipeline.cpp:866-872`).

## 2. Projection / near–far

- Constants (`llmath/llcamera.h:38-47`): `DEFAULT_NEAR_PLANE 0.25`,
  `MIN_NEAR_PLANE 0.1`, `MAX_NEAR_PLANE 1023.9`, `MAX_FAR_CLIP 512`.
  `setNear()` clamps to `[MIN, MAX]` (`llcamera.cpp:136-138`).
- Projection built in `llviewercamera.cpp:296-378` (`setPerspective`, GLM
  `glm::perspective` at `:378`).
- **Near-clip is a single mutable value with many writers** — any A2.1 change
  must account for all of them:
  - `llviewerdisplay.cpp:671` — forced to `MIN_NEAR_PLANE` (0.1) per frame
    when zoomed.
  - `llagentcamera.cpp:217` — `setNear(0.1f)`.
  - `llviewerwindow.cpp:5880` — snapshot/cube-snapshot path.
  - `llmorphview.cpp:85,98` (appearance editor), `llglsandbox.cpp:153,171,288`
    (selection drag), reflection/hero probes carry their own near
    (`llreflectionmap.h:55`, `llheroprobemanager.h:119`).
- Shaders receive `near_clip = getNear() * 2.f` (`pipeline.cpp:9263`) and
  linearize with `deferredUtil.glsl:177-186` (`linearDepth`,
  `linearDepth01`).

## 3. Population — who writes depth

- **Opaque + alpha-masked geometry**: deferred G-buffer pass,
  `llviewerdisplay.cpp:983-1030` (`renderGeomDeferred`). Optional depth
  pre-pass behind `RenderDepthPrePass` (default off, `:1005-1027`).
- **Blended alpha does NOT write scene depth** in the normal path. The gate is
  `lldrawpoolalpha.cpp:239-247`:
  `write_depth = rigged || sSkipScreenCopy || sImpostorRenderAlphaDepthPass ||
  (type == POOL_ALPHA_PRE_WATER)` → post-water blended alpha renders with
  depth test on, depth write **off**.
- **DoF-only alpha depth pass** (`lldrawpoolalpha.cpp:209-227`): when
  `RenderDepthOfField` is on, post-water alpha is re-rendered color-masked
  with `setMinimumAlpha(0.33)` — faces ≳33% opaque write depth so DoF can see
  them. This is the only blended-alpha route into the depth texture.
- Emissive/additive sub-passes never write depth (`lldrawpoolalpha.cpp:510,534`).

## 4. Consumers — who reads depth

All consumers reconstruct via the shared helpers in
`class1/deferred/deferredUtil.glsl` (`getPosition` `:308-317`,
`getPositionWithDepth` `:326-331`, `linearDepth` `:177`) driven by the
auto-derived `inv_proj` uniform (`llrender.cpp:1025-1028`).

| Consumer | GLSL | Bind site |
|----------|------|-----------|
| SSAO | `class2/deferred/sunLightSSAOF.glsl` + `class1/deferred/aoUtil.glsl:27-45` | `pipeline.cpp:9241-9256` |
| Soften light / haze / shadow gather | `class3/deferred/softenLightF.glsl:63,76,122-123` | `pipeline.cpp:9126-9138` |
| DoF (CoC + combine) | `class1/deferred/cofF.glsl:31-65`, `dofCombineF.glsl:36-59` | `pipeline.cpp:8703-8705` (no-DoF path `:8538`) |
| SSR | `class3/deferred/screenSpaceReflUtil.glsl:27,71-73,108,162` | via `screenSpaceReflPostF.glsl` |
| Water/underwater haze | `class3/deferred/hazeF.glsl:52-53`, `waterHazeF.glsl` | `pipeline.cpp:9852,9904` |
| Light volumes | `class3/deferred/{spot,point,multiPoint}LightF.glsl` | `bindDeferredShader` |

## 5. Depth snapshot support (basis of the sanity test)

Already user-facing: snapshot floater type `depth`/`depth24`
(`llfloatersnapshot.cpp:145-148`, `llsnapshotmodel.h:53-55`). Capture in
`llviewerwindow.cpp:5460-5665` (`rawSnapshot`):

- Reads `GL_DEPTH_COMPONENT` as float, linearizes:
  `linear = 1 / (f1 - d * f2)` with
  `f1 = (far+near)/(2*far*near)`, `f2 = (far-near)/(2*far*near)`
  (`:5548-5549`, `:5626`, `:5656`).
- **DEPTH24**: `F32_to_U32(linear, near, far)` (`llquantize.h:111-119`) —
  clamps to `[near,far]`, normalizes, rounds to 24-bit — packed **R = high
  byte, G = mid, B = low** (`:5629-5635`).
  Decode: `meters = near + ((R*65536 + G*256 + B) / 16777215) * (far - near)`.
- **DEPTH**: same linearization quantized to one byte across all channels.
- Debug view: `RenderBufferVisualization` 0-3 blits G-buffer attachments
  (`pipeline.cpp:8889-8918`); there is no dedicated linearized depth debug
  view.

## 6. Repeatable depth-distribution sanity test

Tool: `scripts/perf/bdmerge_depth_compare.py` (stdlib-only, decodes DEPTH24
PNG/BMP pairs, compares percentile distribution).

Procedure (once per depth-touching item, before/after):

1. In-world, pick a repeatable pose: sit the avatar on a fixed prim at a
   fixed camera preset, scene containing near geometry (<1 m), mid geometry,
   far terrain/sky, and at least one blended-alpha object and one particle
   source. Freeze conditions: static region, fixed EEP preset, fixed window
   size, `RenderDepthOfField` **off** (so the alpha DoF depth pass doesn't
   run) for run A, and repeat with it **on** for run B.
2. Snapshot → Save to disk, format PNG, type **depth24**, at window
   resolution. Name `depth_baseline_<item>_{dofoff,dofon}.png`.
3. After the item's change, recapture identically →
   `depth_after_<item>_{dofoff,dofon}.png`.
4. `python scripts/perf/bdmerge_depth_compare.py baseline.png after.png
   --near 0.1 --far 512` — exits non-zero if the depth distribution moved
   beyond tolerance (default: any of p1/p5/p25/p50/p75/p95/p99 shifted >1%
   of the [near,far] range, or >2% of pixels changed bucket).
5. Expected-to-change items (A2.1 near-clip, A2.2 alpha exclusion) must state
   *which* percentiles are allowed to move in their patch-log entry; the test
   still guards the rest of the distribution.

**Baseline reference captures are owed at the next in-world session** — store
them under `doc/depth_baseline/` (not committed if large; record their
SHA-256 + capture conditions in the patch log).
