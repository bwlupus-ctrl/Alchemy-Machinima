# Upstream weather inventory

Imported 2026-10-25 from the supplied `I:/alchemy-machinima/slipstream/` subtree.
The containing repository reports `330d012e1612a3f4779a01dcc0614a91939ead0f`; this does not independently pin the subtree.
SHA-256 below pins the exact source bytes read at import. Original notices are retained.

| Source path | SHA-256 | Destination | Licence | Changes |
|---|---|---|---|---|
| `indra/newview/slweather.cpp` | `a695406074725bd4aae3ac9f7230abf7f4e2faa8cc517015dab5e39c181236b3` | `indra/newview/slweather.cpp` | LGPL 2.1 only, viewerlgpl | Injected settings/time/place seams; CPU-only; accumulating depth snap correction; canonical successful teleports. |
| `indra/newview/slweather.h` | `ce15fd366dc92a8ff858ec74736d8cafa2e6b937e4949b354597511be7bbf4bb` | `indra/newview/slweather.h` | LGPL 2.1 only, viewerlgpl | Injected settings/time/place seams; CPU-only; accumulating depth snap correction; canonical successful teleports. |

Rate presets use the documented public rates; no upstream floater code or renderer code was copied. Future audio/assets and protocol files remain deferred to their owning phases.

## WP2 reference-only inspection (2026-10-25)

| Source path | SHA-256 | Use / notice handling |
|---|---|---|
| `indra/vkclean/shaders/clean_weather_surface.glsl` | `d4735c72af6a5e0a0cad4f53a857c3679fe868e8e375fcd2093a4804501bfb58` | Reference design for material-before-lighting, sky exposure, puddles and ripples. No explicit licence/copyright notice in the inspected shader header; no source text or Vulkan ABI copied. Native `weatherMaterialF.glsl` is a new viewerlgpl implementation with Slipstream design credit. |
| `indra/newview/vkgfx/llcleanweathervk_accum.cpp` | `1a1c41dfbe4756b43f7924b87cb8791cb30a9f380afbf0af0462f838bc9f7b7a` | Inspected lifetime/accumulation design only; no renderer code copied or avatar accumulation imported. Its inspected header describes Slipstream avatar accumulation and does not contain an explicit licence notice. |

The containing repository revision remains the one recorded above; hashes identify
the inspected files independently. WP2 extends the already credited `slweather`
CPU integration and retains its complete original licence and authorship headers.

## WP3 reference-only inspection (2026-10-25)

| Source path | SHA-256 | Use / notice handling |
|---|---|---|
| `indra/newview/vkgfx/llcleanweathervk.cpp` | `7534a6894f78d49e522400f9d1c491d532463ee9c7f56bc0d13e172ad67898f2` | Bounded particle ownership, lighting and coverage design reference only. Inspected header has no explicit licence notice; no renderer code copied. |
| `indra/vkclean/shaders/clean_weather_sim.comp` | `499122de7864504b01fc5eb44de4879bd7d64afc05c522aa19310807ac6ed54a` | GPU list evaluation and caps reference only. New GL world-cell evaluator replaces mutable camera-box particle state; no Vulkan ABI, append/subgroup or shader source copied. No explicit licence notice in inspected shader. |
| `indra/vkclean/shaders/clean_weather.vert` | `1fdab3e8d5a644b06aea2cb6a1b210b5fcf237ed10b0ed533217117c5ebfa7ed` | Instanced billboards and subpixel coverage design reference only; new native vertex shader. No explicit licence notice in inspected shader. |
| `indra/vkclean/shaders/clean_weather.frag` | `cf6a13e434cd82599b14c31f6417509540fd561f818179d6f418f2b8dae6816b` | Premultiplied lit flakes design reference only; new native depth-softened fragment shader. No explicit licence notice in inspected shader. |
| `indra/vkclean/shaders/clean_weather_veil.frag` | `a4525ab7247ac0e26dc3d6717ecee45bed894fac58ae58a72ad60a8ff104c8a1` | Optional single physical extinction pass at open scene depth; new native GL implementation. No explicit licence notice in inspected shader. |
| `indra/vkclean/shaders/clean_weather_veil.glsl` | `77e01eac1d7abf52250b2030d89818d5613698a5c02ce488f52a1162d1e1ac3b` | World-space openness/unknown-coverage design reference only. No volumetric injection or Vulkan bindings copied. No explicit licence notice in inspected shader. |

Destinations: `alweatherparticles.{cpp,h}`, `weatherSnowSim.glsl`,
`weatherSnowV/F.glsl`, `weatherPrecipCoverF.glsl`, and
`weatherPrecipVeilF.glsl`. These are new viewerlgpl GL implementations with
Slipstream design credit in their headers. The upstream containing revision is
recorded above; these per-file hashes pin the inspected reference bytes.
`slweather`'s existing LGPL 2.1-only notices and physical extinction/rate methods
remain intact. No private, Reeelight, ReShade or iMMERSE source was inspected.
