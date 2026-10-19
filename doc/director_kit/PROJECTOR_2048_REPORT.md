# Minimal 2048 projector resolution - fix round 3

Resolution offers **256 / 512 / 1024 / 2048**. Scene persistence accepts and
round-trips 2048. Only an occupied 2048 projector using **This camera** raises
its own camera's retained output capacity to 2048. Capacity depends on the
consumer binding and capture generation, independent of frame demand or Gate
watch counters. Legacy sizing arithmetic, Quality ceiling, Adaptive and video
output/decode sizing remain unchanged. The kit pin remains **37 pages / 2143
rows**.

The old Gate has no 2048 capacity path. Gate watch, incoming and Program demand
remain capped at HEAD's 1024 capacity unless that camera independently has its
own This camera 2048 consumer. A **Feed from: Gate program** projector authored
at 2048 renders a 1024 cookie: demand, watch sizing, fade-budget inputs, feed
settings and composite dimensions clamp to 1024. The authored value remains
2048 for persistence and switching back to This camera. Clamping does not reject
the feed or introduce a black/no-signal result. Gate-subscribed faces retain
HEAD behavior. `llprismgate.cpp` is identical to HEAD: its 1024 cookie
qualification, **8M total pixel budget**, eight-pass limit and shader
qualification are unchanged. The visible-face/watch ordering is unchanged;
there is no longer any Gate 2048 demand for it to suppress.

Picture now uses the camera's consumer capacity: for example, **2048 x 1152 px**
at 16:9, **1152 x 2048 px** at 9:16, and **512 x 2048 px** at aspect 0.25
while its own 2048 consumer exists. The 2048 readout shares the renderer's
max-axis sizing arithmetic. Without that consumer, the text is
identical to HEAD, including its scale display. Picture help describes the
conditional cap; Resolution help states the Gate clamp.

VRAM calculations (not runtime measurements):

- **No This camera 2048 consumer: +0 MiB versus HEAD.** Lazy retained outputs
  remain 1024 RGBA16F, 8 MiB each; Gate 2048 subscriptions do not increase them.
- **This camera 2048 consumer:** only its camera slot uses a 32 MiB output,
  **+24 MiB per camera**. Its mipmapped RGBA8 cookie uses 21.33 MiB,
  **+16 MiB versus 1024**. Exact-size scratch depends on camera aspect and
  rendering features.
- Capacity changes preserve the held camera picture for every consumer while
  the source is offline or rendering fails. The first successful render produces
  a spare at the new capacity and swaps its owned storage into the stable output
  object. This applies to growth and shrink; the old allocation is released only
  after replacement. An offline camera can therefore temporarily retain its
  32 MiB held output after its last 2048 consumer goes away.
- Lowering settings releases an oversized mipmapped cookie directly from the
  configuration dirty path, independent of feed availability. Removal and
  reassignment already release their cookies directly. Large shared scratch is
  released when no camera needs it. Video output/decode sizes remain at HEAD.

| Check | Result |
| --- | --- |
| llprismprojector compile/link/run | PASS, 49/49 |
| llprismgate compile/link/run | PASS, 64/64 |
| zs_check: llprismlens.cpp, pipeline.cpp, alpaneldirectorvcam.cpp, llprismgate.cpp, tests/llprismprojector_test.cpp, and llviewerdisplay.cpp (includer for both touched headers); -IncludeTests | PASS, 6/6 |
| Projector test syntax using its integration-target flags | PASS |
| director_kit_gen.py generation / --check | PASS |
| director_page_sources.py generation / --check | PASS |
| director_functional_manifest.py generation / --check | PASS |
| prebuild_guard.ps1 -ChecksOnly | PASS |
| git diff --check | PASS |

Prebuild passed UI layout, UI draw, compile, light-map maths, kit generation,
kit lint, parity, layout self-test, page sources, theme check and theme self-test.

Tests retain allocation-only-with-consumer, legacy sizing, persistence and
release coverage. The 2048 stability test runs **300 frames** through the same
per-frame capacity-request helper used by the renderer, then production/reuse, and
asserts no release or reallocation while the consumer exists. It checks held
pictures across 300 offline growth frames, allocation/render failure, first
successful replacement, second-consumer retention, and held output followed by
shrink after removal/reassignment/lowering. Additional tests cover offline
oversized-cookie cleanup and all three Picture aspects, with legacy text intact.
Gate coverage now asserts 1024 capacity for Program/incoming cameras, a 2048
subscription clamping to a usable 1024 cookie, unchanged legacy resolutions,
and HEAD's fade qualification and budget. No tests or gates were weakened.

Unit targets used fresh `.tmp/projector_2048_r3_obj` and
`.tmp/projector_2048_r3_bin` directories, existing dependency archives and the
previously rebuilt llimagej2coj archive. Gate's normal Build target hit an
existing deployment event pointing to a missing sharedlibs test executable;
the freshly linked test executable passed despite that post-build event. No viewer target was built.

Required regeneration updates shared hash stamps and Rim manifest metadata
(**32 to 40** parsed presets; the sentinel is unchanged). The user's one-line regex fix
in `director_functional_manifest.py` is preserved; Rim preset source is unchanged.
Evidence: `.tmp/projector_2048_r3_*.log`. Prebuild uses
GIT_CONFIG_COUNT=1, GIT_CONFIG_KEY_0=core.safecrlf,
GIT_CONFIG_VALUE_0=false.

No viewer build, in-world/GL measurement, commit, push or stash operation occurred.
Round 4: Gate subscription now publishes its state before dirtying the feed, releasing an offline 2048 cookie before bind can reuse it; subscription-path regression PASS (projector 50/50, Gate 64/64), zs_check 6/6 with -IncludeTests, prebuild_guard -ChecksOnly with the required safecrlf environment, and git diff --check PASS (Gate retains the known post-link deployment-path error; its fresh executable passes).
