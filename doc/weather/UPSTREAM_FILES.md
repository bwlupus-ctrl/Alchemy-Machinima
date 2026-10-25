# Upstream weather inventory

Imported 2026-10-25 from the supplied `I:/alchemy-machinima/slipstream/` subtree.
The containing repository reports `330d012e1612a3f4779a01dcc0614a91939ead0f`; this does not independently pin the subtree.
SHA-256 below pins the exact source bytes read at import. Original notices are retained.

| Source path | SHA-256 | Destination | Licence | Changes |
|---|---|---|---|---|
| `indra/newview/slweather.cpp` | `a695406074725bd4aae3ac9f7230abf7f4e2faa8cc517015dab5e39c181236b3` | `indra/newview/slweather.cpp` | LGPL 2.1 only, viewerlgpl | Injected settings/time/place seams; CPU-only; accumulating depth snap correction; canonical successful teleports. |
| `indra/newview/slweather.h` | `ce15fd366dc92a8ff858ec74736d8cafa2e6b937e4949b354597511be7bbf4bb` | `indra/newview/slweather.h` | LGPL 2.1 only, viewerlgpl | Injected settings/time/place seams; CPU-only; accumulating depth snap correction; canonical successful teleports. |

Rate presets use the documented public rates; no upstream floater code or renderer code was copied. Future audio/assets and protocol files remain deferred to their owning phases.
