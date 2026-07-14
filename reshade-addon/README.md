# sl_reshade_bridge — ReShade add-on for Alchemy-Machinima (SL viewer)

Feeds the viewer's G-buffer and camera state to ReShade effects. Works together
with the viewer's exported `SLReShade_GetFrame` (see
`indra/newview/llreshadebridgeabi.h`, the single shared contract). The ABI header
and this add-on are byte-identical to the phoenix-reshade-XL copy — one `.addon`
serves both forks; only the viewer side differs (this fork also feeds motion).

## Architecture

```
firestorm-bin.exe ──exports──> SLReShade_GetFrame()          (viewer side)
        ▲                              │
        │ injected                     │ GetProcAddress, once
        ▼                              ▼
ReShade (opengl32.dll) ──loads──> sl_reshade_bridge.addon    (this project)
```

Each frame (in `reshade_begin_effects`) the add-on:
1. reads the published `SLReShadeFrame` (seqlock-protected),
2. copies viewer G-buffer textures into **add-on-owned immutable resources**
   (`device::create_resource` + `copy_texture_region`) — never binds the
   viewer's mutable textures directly (they are mipmap-incomplete under
   ReShade's samplers and would sample black),
3. binds them to FX semantics, and pushes camera uniforms.

Depth is intentionally **not** provided — use ReShade's built-in
`generic_depth` add-on (enable *Copy depth buffer before clear operations* for
SL).

## Consuming from an .fx shader

Don't hand-declare the semantics — `#include "SL_Bridge.fxh"` (ships in
`shaders/`). It is the single source of truth for the SL_* textures, the sl_*
camera uniforms, buffer **orientation**, and the normal decode:

```hlsl
#include "ReShade.fxh"
#include "SL_Bridge.fxh"
// ... then use the orientation-correct fetches:
float3 n_view = SL_NormalView(uv);   // decoded, view-space
float3 albedo = SL_Albedo(uv);
float2 motion = SL_Motion(uv);       // NDC delta, Y-sign corrected
```

The SL viewer is OpenGL (bottom-left texture origin); ReShade samples top-left,
so `SL_Bridge.fxh` flips V (`SL_INPUT_IS_UPSIDE_DOWN`, default 1 — the analog of
`RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN`). **Keep the two matched** so SL normals and
generic_depth's DEPTH land in the same orientation. The add-on does NOT flip:
`copy_texture_region` can't mirror, and orientation is a sampling convention.

## iMMERSE integration (SL_GBufferProvider.fx)

`shaders/SL_GBufferProvider.fx` overrides iMMERSE's normals with real SL gbuffer
normals so RTGI/MXAO use true geometry instead of depth-derived normals.

- **Install:** copy it into the iMMERSE shader folder (next to
  `MartysMods_LAUNCHPAD.fx`) so its `MartysMods/…` includes resolve; keep
  `SL_Bridge.fxh` on an effect search path.
- **Order** in the ReShade effect list: `Launchpad` → `SL G-Buffer Provider` →
  `RTGI/MXAO`. Above Launchpad, Launchpad overwrites it.
- **How:** includes Marty's public `mmx_deferred.fxh` (same interface RTGI uses,
  no code copied) and renders `octahedral_enc(-N_view)` into
  `Deferred::NormalsTexV3`. GL eye-space → view-space is a Z-negate
  (`SL_NORMAL_FLIP_Z`, default 1; flip to 0 if RTGI lighting looks inverted).
- **Safety:** if the add-on isn't feeding, every pixel discards → pure no-op,
  RTGI falls back to Launchpad normals.
- **Motion:** `SL_PROVIDE_MOTION` (default 1) writes `Deferred::MotionVectorsTex`
  from the velocity buffer. Enable `BDMergeVelocityBuffer` in the viewer to feed
  it; the pass discards where the velocity delta is zero, so it is a no-op when
  the buffer is off. Tune sign/scale live via the Motion controls.

## Recommended Launchpad settings (performance)

Because the provider **overwrites** Launchpad's motion and detail normals, you
otherwise pay for work that is immediately thrown away. Launchpad is proprietary
(do NOT modify it); instead reclaim the cost through its own UI:

- **Motion Estimation → Flow Quality → `Low`.** Its optical-flow result is
  discarded and replaced by our real motion, so there's no reason to pay for a
  high-quality estimate. Reclaims most of Launchpad's per-frame cost.
- **Normal Maps → Enable Texture Normals → `Off`.** It estimates fake relief
  into the XY normals we then overwrite with real ones — pure waste.
- **Normal Maps → Enable Smooth Normals → `On`.** Keep this; it smooths the ZW
  geometry normals the provider deliberately preserves, improving RTGI stability.

(Launchpad still allocates its flow buffers regardless of quality, so this
recovers GPU time, not memory. Modifying Launchpad to drop them is not permitted
by its license.)

For a lean production build set `SL_ENABLE_DEBUG = 0` (global preprocessor
definition) to compile out the on-screen debug readback pass entirely.

## Building

Standalone; does not touch the viewer build.

```
cmake -S reshade-addon -B reshade-addon/build -G "Visual Studio 18 2026" -A x64
cmake --build reshade-addon/build --config Release
```

or a bare `cl` from a VS x64 dev prompt:

```
cl /nologo /std:c++17 /O2 /W4 /EHsc /LD src\sl_reshade_bridge.cpp ^
   /I external\reshade /Fe:sl_reshade_bridge.addon
```

## Installing

1. Build the viewer (it must export `SLReShade_GetFrame` — any build after the
   ABI landed does).
2. Install ReShade 6.x **with full add-on support** on `firestorm-bin.exe`
   (OpenGL).
3. Copy `sl_reshade_bridge.addon` next to the game's ReShade DLL (or into the
   `AddonPath` configured in `ReShade.ini`).
4. Check the ReShade overlay → Add-ons tab: **SL G-Buffer Bridge** should be
   listed, and its log lines are tagged `[SLBridge]` in `ReShade.log`.

If the viewer build lacks the export, the add-on logs one warning and stays
inert; everything else keeps working.

## Vendored SDK

`external/reshade/` is the ReShade add-on SDK (Apache-2.0), vendored from the
`archive/reshade-rtgi-bridge` branch of the sibling alchemy repo (originally
from crosire/reshade). Update by copying the `include/reshade*.hpp` set from a
newer ReShade source tree.
