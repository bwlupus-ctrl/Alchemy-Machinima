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
