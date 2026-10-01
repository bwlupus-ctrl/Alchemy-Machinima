# Machinima VRAM Governor

## Purpose

The Machinima client now exposes an Auto/Custom VRAM cap and manages the
discardable texture pool against it. This is an OpenGL residency governor, not
a hard per-process allocator limit: OpenGL cannot provide the same process
budget contract as DXGI. The implementation combines viewer-side allocation
accounting with adapter-wide driver pressure where the driver exposes it.

The control is in **Preferences → Graphics → Advanced**, under General:

- `0` means **Auto**. The cap is detected dedicated VRAM divided by
  `RenderTextureVRAMDivisor` (currently `1` by default).
- Any nonzero value is a **Custom** cap in MB.
- The control takes effect live and is limited to the detected adapter VRAM.

The UI also displays tracked use, effective cap, pressure state, reducible
texture bytes, and driver-reported free VRAM when available.

The live telemetry is presented in a dedicated full-width status panel rather
than being wrapped into the General controls. It shows requested, configured,
and effective caps separately so a user can immediately tell whether a custom
value was accepted or raised above the fixed-allocation floor.

`LLSliderCtrl` is floating-point internally while `RenderMaxVRAMBudget` is a
`U32`. The commit callback therefore rewrites the slider value explicitly with
`setU32`, and the governor reads the current `U32` setting directly each frame.
This avoids a live edit being displayed by the slider while a typed cached
control remains in Auto mode.

## Accounting model

Every tracked GL texture name owns one `LLTextureAllocation` record:

```text
base       = level-zero bytes × face/slice count
estimated  = base plus every mip level when the allocation has mipmaps
mipmapped  = estimated for mip-bearing allocations, otherwise zero
```

Mip levels are summed to 1×1 with `width > 1 || height > 1`, including
non-square chains. Driver-padded formats use `dataFormatVRAMBytes`; generic
`GL_COMPRESSED_*` formats use a conservative 4-bpp or 8-bpp block-compression
estimate. Explicit DXT uploads, generated mipmaps, manual mip chains, cubemaps,
cubemap arrays, bump maps, render targets, and texture resize paths participate
in the same allocation/deletion accounting.

The governed totals are:

```text
tracked   = estimated GL textures + vertex buffers
reducible = estimated bytes belonging to mip-bearing textures
fixed     = max(tracked - reducible, 0)
```

`LLRenderTarget::sBytesAllocated` is reported separately for diagnosis but is
not added to `tracked`, because render-target textures are already counted by
`LLImageGL`.

Accounting is an estimate of allocations known to the viewer. Driver page
alignment, descriptors, allocations made by libraries outside the wrappers,
and other applications are not precisely attributable. Driver headroom is the
backstop for those gaps.

## Effective cap and fixed-allocation floor

The configured cap is clamped from 768 MB through detected VRAM. Texture
discard cannot reclaim render targets, vertex buffers, or no-mip textures. If
those fixed allocations make the configured cap impossible, the governor uses:

```text
effective cap = max(configured cap, fixed + 256 MB)
```

The UI reports when this adjustment is active. Render-target resolution or
quality is never silently changed by the governor.

## Pressure state machine

Pressure is calculated against the elastic pool:

```text
cap usage = reducible / max(effective cap - fixed, 1 MB)
```

Entry is immediate:

| State | Viewer allocation signal | Driver/system signal | Bias floor |
|---|---:|---|---:|
| Normal | below 85% | healthy | 1.0 |
| Pressure | at least 85% | driver free below reserve | 1.5 |
| Critical | at least 95% | driver free below half reserve, a new NVIDIA eviction, or low system memory | 2.5 |

The driver reserve is 10% of detected VRAM, clamped to 256–1024 MB.

Recovery uses hysteresis and descends exactly one state per stable interval:

- Critical → Pressure: five seconds below 90%, healthy system RAM, and driver
  headroom above 75% of reserve.
- Pressure → Normal: a separate five seconds below 80%, healthy system RAM,
  and driver headroom above 125% of reserve.

Only currently observed pressure can continue increasing discard bias. During
a recovery hold the bias can decay toward the still-latched state's floor; this
avoids the prior failure mode where a resolved spike drove bias to 4.0 for tens
of seconds.

## Driver telemetry

Queries run from `LLViewerTexture::updateClass()` on the render thread, at most
once per second, and only after the GL manager is initialized:

- NVIDIA `GL_NVX_gpu_memory_info`: current available video memory and eviction
  count.
- AMD/ATI `GL_ATI_meminfo`: current texture-pool free memory.
- Unsupported or failed queries remain **Unavailable** and do not become a
  false zero-free-memory Critical signal.

These values are adapter-wide. Multiple viewer instances can each remain below
their configured cap while collectively exhausting the adapter; the shared
driver-free signal is what makes each instance respond to that condition.

## Machinima capture behavior

The Machinima capture pin is preserved without making the user-selected cap
advisory. While a take is active and the governor is Normal, capture pins
discard bias to 1.0 and requests full-resolution textures. At Pressure (85%) or
Critical (95%), the cap takes ownership: the bias floor is retained, both
capture full-resolution request paths are released, and `TextureLoadFullRes`
also yields to the governor. The capture pin remains armed and restores full
quality automatically only after hysteretic recovery returns to Normal.

The Advanced Graphics status title reports `CAPTURE QUALITY PIN` while capture
quality owns the texture policy and `CAP ENFORCED DURING CAPTURE` when pressure
has transferred ownership to the governor.

## Main implementation files

- `indra/llrender/llimagegl.cpp` / `.h` — allocation records and resident-byte
  estimates.
- `indra/llrender/llcubemap.cpp`, `llcubemaparray.cpp`, `llrendertarget.cpp` —
  multi-surface/mipmap coverage and allocation-failure symmetry.
- `indra/newview/llviewertexture.cpp` / `.h` — budget, driver telemetry,
  pressure state machine, hysteresis, and public runtime statistics.
- `indra/newview/llfloaterpreferencesgraphicsadvanced.cpp` / `.h` and
  `skins/default/xui/en/floater_preferences_graphics_advanced.xml` — cap and
  live status UI in the Machinima advanced graphics floater.
- `indra/newview/lltextureview.cpp` — developer texture-console telemetry.

## Runtime validation still required

Compile/link proves integration, not residency behavior. Validate on hardware:

1. Set Custom caps around the live working set and cross 85% and 95%; verify
   immediate Pressure/Critical entry and gradual texture downrez.
2. Remove the load and verify separate five-second recovery rungs and bias
   decay without a long 4.0 overshoot.
3. Exercise 4K shadows, mirrors, reflection probes, volumetrics, and post
   processing; confirm fixed-floor adjustment is reported without RT resizing.
4. Run two client instances and verify adapter-wide low-free-memory pressure is
   detected even when each viewer is below its own accounting cap.
5. Validate NVIDIA eviction changes, AMD free-memory behavior, and the
   unavailable fallback on unsupported hardware.
6. Force or simulate color/depth render-target allocation failures and verify
   texture and `sBytesAllocated` counters remain symmetric.
7. Test capture mode separately: confirm full quality at Normal, automatic cap
   enforcement at Pressure/Critical, and quality restoration only after the
   state machine recovers to Normal.
