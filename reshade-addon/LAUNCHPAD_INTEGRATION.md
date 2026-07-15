# Integrating the SL bridge into a local Launchpad — V4 edition (advanced / personal use)

> ⚠️ **License:** `MartysMods_LAUNCHPAD.fx` is proprietary (© Pascal Gilcher,
> "all rights reserved, unauthorized copying prohibited"). A modified Launchpad
> **cannot be distributed** and must be re-applied on every iMMERSE update. The
> shippable path is the standalone `SL_GBufferProvider.fx`. This guide is only
> for a personal build that fully *replaces* Launchpad's optical flow to reclaim
> its passes and texture memory. All code here is our own; Marty's passes are
> referenced only by single-line anchors.

This targets the **iMMERSE "Deferred V4" drop (2026)** — the one with
`calc_flow_dynamic`, IPC feature gating, split `RTGI_DIFFUSE`/`RTGI_SPECULAR`,
and the `NormalsTexV4`/`GeoNormalsTexV4` deferred interface. For the old V3
interface see git history of this file.

---

## 0. Re-applying after an iMMERSE update (the short version)

The whole graft is automated:

```
python tools/graft_launchpad_v4.py
```

Point `PATH` inside the script at a **pristine V4** `MartysMods_LAUNCHPAD.fx`
(back it up first). Every insertion is anchored and asserted — if Marty moved
things around in a newer drop, the script aborts on the first stale anchor
instead of producing a half-grafted file. Fix the anchor, re-run.

What it produces, in order:

| # | What | Where |
|---|------|-------|
| A1 | `SL_ENABLE_BRIDGE` / `SL_PROVIDE_ALBEDO` / `SL_ALBEDO_TO_LINEAR` / `SL_NORMAL_FLIP_Z` preprocessor block | after `LAUNCHPAD_DEBUG_OUTPUT` |
| A2 | `OPTICAL_FLOW_Q` uniform compiled out under bridge | UI section |
| A3 | `SL_DEBUG_VIEW`, `SL_MOTION_SCALE/FLIP_X/FLIP_Y` uniforms | before `NORMALS_MODE` |
| A4 | `#include "SL_Bridge.fxh"` | after the mmx includes |
| A5 | ALL optical-flow **textures** compiled out (`LinearDepth*`, `MotionTexLA/LB0-7`, `MotionTexUpscale*`, `FlowFeatures*`) | texture section |
| A6 | ALL optical-flow **shaders** compiled out (`WriteCurrFeatureAndDepthPS` … `UpscaleFilter2to1PS`) | OF section |
| A7 | bridge providers: `PS_ProvideNormals` / `PS_ProvideMotion` / `PS_ProvideAlbedo` / `PS_ShowReceived` | before the Normals entry points |
| A8 | technique: OF pass block → single `SLMotion` pass | technique |
| A9 | `SLAlbedo` overlay pass (writemask 7) | after `UpscaleAlbedoPyramid` |
| A10 | `SLNormals` overlay pass | after `CopyNormals`, before `IPC_CLEAR()` |
| A11 | technique label gets `[SL BRIDGE]` so the UI shows which build loaded | technique |
| A12 | standalone `SL_Bridge_Debug` technique | end of file |

---

## 1. What changed vs the V3 graft (why the code differs)

### Normals: two textures now
V3 packed shading normals (`.xy`) and geometry normals (`.zw`) into one RGBA16
`NormalsTexV3`; we wrote `.xy` only via `RenderTargetWriteMask = 3`. V4 splits
them into two RG16 octahedral textures. The override now simply renders into
`NormalsTexV4` (full write, no mask) and **does not touch** `GeoNormalsTexV4` —
same policy, new mechanics: SL gbuffer normals (which include normal maps)
replace the shading normal; Launchpad's smoothed geometry normal survives so
RTGI keeps its stable coarse structure.

### Albedo: depth lives in alpha now
V4's `AlbedoTex.a` carries linear Z ("simplifies textured normals generation" —
`CopyNormalsPS` reads it). The albedo overlay pass therefore uses
`RenderTargetWriteMask = 7` to replace RGB only. Without the mask, textured
normals silently break.

### IPC gating: providers are gated too
V4 Launchpad only runs a feature's passes when a downstream effect requested it
via `IPC_REQUEST_FEATURE` (read back through `OpticalFlowVS`/`AlbedoVS`/
`NormalsVS`, which NaN the fullscreen triangle when unrequested). The bridge
passes use the **same gating vertex shaders**, so e.g. `SLMotion` costs nothing
in a preset with no temporal effect enabled. The overlay passes must sit
**before** `IPC_CLEAR()` in the technique (the script places them correctly).

### Motion convention: derived, baked in
`velocityF.glsl` writes `cur_ndc − last_ndc` in GL NDC (x right, y **up**),
un-jittered. iMMERSE wants `prev_uv − cur_uv` in ReShade UV (y **down**):

```
prev−cur = −v;   Δuv = Δndc × 0.5;   top-left flip negates y once more
⇒ motion = float2(−v.x, +v.y) × 0.5
```

`SL_MOTION_FLIP_X = true`, `FLIP_Y = false`, `SCALE = 1.0` defaults encode
exactly this; the uniforms remain only as a live escape hatch.

### Stale-motion rule (unchanged, still REQUIRED)
With OF removed, `PS_ProvideMotion` is the *sole writer* of
`Deferred::MotionVectorsTex`, which is never cleared. It must **always write** —
a discard on zero-delta freezes last frame's motion on static pixels and RTGI
smears when the camera stops. (The **normals/albedo** overrides keep their
`discard` on purpose: their Launchpad writers still run, so discard = graceful
fallback.)

---

## 2. Viewer-side requirements

- `BDMergeVelocityBuffer` **must be ON** — with OF gone there is no estimated
  fallback; an unbound `SL_MOTION_NDC` reads zero motion everywhere.
- **[BDMerge A5.4-1c]** the viewer now fills the velocity buffer with a
  fullscreen **camera-motion fallback** (reprojected from scene depth) before
  the geometry pass stamps true per-object motion. So avatars/rigged meshes
  (until Phase 1b), sky, and excluded alpha all carry correct camera-induced
  motion instead of reading "static". Avatar *limb/body* motion still reads
  zero until Phase 1b (previous-frame skin matrices).
- Do NOT change `velocityF.glsl`'s output convention — SMAA T2x consumes the
  same buffer (`SMAA_DECODE_VELOCITY`); the NDC→iMMERSE conversion belongs in
  the provider shader, once.

## 3. Debug / validation

- Enable **SL_Bridge_Debug**, drag it to the **absolute bottom** of the effect
  list, then drive it with the **`DEBUG view` combo under MartysMods_Launchpad's
  settings** (category "Second Life Bridge") — both techniques share one .fx's
  uniforms, so the control is NOT under the SL_Bridge_Debug entry.
- Normals view: should match `SL_BridgeDebug` "Normals (decoded)" with blue
  inverted (Z-flip). Blocky depth-derived normals = wrong pass order.
- Motion view: hue = direction, brightness = speed. Pan → coherent field;
  stop → neutral gray immediately (stale-motion rule working). A moving avatar
  should read *differently* from the wall behind it once Phase 1b lands; today
  it reads camera motion only.
- Albedo view: flat unlit surface color; sky keeps Launchpad's estimate.

## 4. Gotchas carried over

- Keep a copy of `SL_Bridge.fxh` **inside `Shaders\iMMERSE\`** (next to
  Launchpad). Search-path resolution of `#include "SL_Bridge.fxh"` can
  intermittently fail on reload (`error X3004: undeclared identifier
  'SLNormalsTex'`), which silently drops Launchpad AND SL_Bridge_Debug from the
  technique list. Keep it in sync with the `Shaders\` copy.
- The albedo pyramid still runs when albedo is requested (we overlay on top of
  it — that's what keeps the sky fallback). Reclaiming those ~20 passes means
  losing the fallback; only worth it with a flat-color replacement. Opt-in,
  not done by the script.
