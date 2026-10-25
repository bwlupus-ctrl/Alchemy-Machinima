WP2 is implemented and uncommitted on `wip/weather-port`; HEAD remains `d9eec7c85c5`. No viewer build or push.

Changed surface resources/coverage, wet-material shaders, bridge/model, pipeline/probes, Director/settings/presets, generated UI, provenance and tests. Ten settings provide thirteen live rows; persistent wet surfaces and puddles default off.

Passed: 28 CPU cases, 40 material off-identity comparisons, 48 WP1 rain fixtures, 18 BRDF integrals, eleven cover fixtures, emission/alpha/flag preservation, normalized ripples, camera stability and observable Director bindings. Preserved mutation evidence rejects all 26 controls; WP1 baseline rejects new behavior. Maximum temporal error: 1.30e-7 (limit 2e-5).

4K off/on: 0.186480/0.212432 ms, added 0.025952; two views 0.339696/0.389408 ms, added 0.049712. Added logical storage: 2.004 MiB.

Generators pass; layout: zero findings; kit lint: zero errors; UI draw lint: zero new findings; diff check: clean. Commands have explicit deadlines. `/Zs` skipped per continuation brief; lead must run it.

Touched `.cpp` files: `indra/llrender/llglslshader.cpp`, `indra/llrender/llshadermgr.cpp`, `indra/newview/alpanelweathersettings.cpp`, `indra/newview/alweatherbridge.cpp`, `indra/newview/alweatherresources.cpp`, `indra/newview/alweathershelter.cpp`, `indra/newview/llactormover.cpp`, `indra/newview/lldrawpool.cpp`, `indra/newview/llfloaterdirector.cpp`, `indra/newview/llreflectionmapmanager.cpp`, `indra/newview/llviewercontrol.cpp`, `indra/newview/llviewerdisplay.cpp`, `indra/newview/llviewershadermgr.cpp`, `indra/newview/pipeline.cpp`, `indra/newview/slweather.cpp`, `indra/newview/tests/alweathershelter_test.cpp`.

Open: **PENDING-QUIET** native scene/p95/state checks, forward-alpha cost, final native build and user acceptance. Blended glass does not seal cover; advanced 1024 cover requires 8 MiB.
