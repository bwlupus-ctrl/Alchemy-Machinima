**GO-after-fixes. No P0 found.** Read `CLAUDE.md` and checked HEAD `c31f37cc43a`. No edits, builds, or commits. The architecture is viable, but the brief is not implementation-ready. Section references below refer to V2.

- **P1 — Default probe never settles (§3.5).** Comparing camera origin against `mStartOrigin` compares positions separated by the capture’s **+64 m Z offset** (`llreflectionmapmanager.cpp:138`). The 16 m threshold therefore fires while stationary. Store camera position separately or compare capture origins consistently.

- **P1 — “Safety-only” texture animation actually generates continuous dirt (§3.4 H3, §3.8).** Texture animation calls drawable `markRebuild(...REBUILD_TCOORD)` (`llvovolume.cpp:673`), which passes H3’s “anything except POSITION” test. This can keep ordinary probes cycling and Live On change from idling. Classify animation-originated rebuilds explicitly; excluding group rebuilds alone does not solve it.

- **P1 — Movement detection uses the wrong signal (§3.4 H1a).** `updateXform()` returns remaining interpolation error, not actual transform displacement. Undamped changes normally return zero; final damped snaps also zero it (`lldrawable.cpp:639–683`). Combined with unchanged-AABB filtering, rotations—including projector direction changes—can disappear. Compare before/after transforms; retain attachment-light events independently. Sixty-second safety is not an acceptable substitute for ordinary light-edit responsiveness.

- **P1 — Completion lacks a transaction-validity contract (§3.1, §3.6).** Deleting the active probe resets `mUpdatingFace`, but **not `mRadiancePass`** (`llreflectionmapmanager.cpp:1238`). A replacement can start in radiance and be acknowledged after six faces without its irradiance pass. Require valid transaction start, allocation epoch, and both completed passes before acknowledgment; invalidate on deletion/reset. Pointer-plus-slot identity also needs protection against reuse. Mid-capture serial retention itself is sound.

- **P1 — Live/realtime records contaminate starvation verdicts (§3.6, §5.4).** Flush dirties all allocated records, including Live, but only ordinary completion acknowledges them. Live is excluded from ordinary selection. Consequently successful Live refreshes can leave permanently pending records and false `STARVED` reports. Separate scheduler ownership in eligibility/statistics. Overflow/resync must explicitly bump Live’s scene serial, including when the event vector is empty.

- **P1 — D does not preserve emissive surfaces (§7).** Opaque and masked emissive PBR materials use the allowed `PASS_GLTF_PBR*` keys (`llvovolume.cpp:7466`). Pass-key inspection cannot establish “non-emissive.” Inspect every draw’s material or retain PBR groups conservatively.

- **P1 — Acceptance gate is insufficient (§0 D1, §9).** T1–T7 omit slicing, Live convergence, deletion, allocation, toggling during capture, and teleport. Require those before flipping the default. T2/T10’s approximately one-second response is impossible with a shared queue; use measured queue-dependent expectations. Safety repairs persistent omissions, not transient flexi/media/animation changes; `MaxAge=0` removes that backstop entirely.

- **P2 — Dynamic policy is reasonable but misdescribed (§0 D3/D7).** Always-eligible dynamic probes avoid unreliable pose hooks. They are not on a guaranteed fixed cycle: queue contention adds latency, and MinInterval imposes at least one second between ordinary starts. Explain this separately from closest-dynamic slicing’s three-frame pass at N=2.

- **P2 — Compile/interface corrections (§3.2, §3.5).** Extracted environment code references `auto_adjust_legacy`, declared outside the block (`llreflectionmapmanager.cpp:756`). Pass or declare it explicitly. An inline recorder accessor cannot read a `.cpp`-private static without a header-visible definition/storage contract.

The face ceilings are structurally credible: ordinary ≤1, secondary ≤6, sliced ≤N. Logs verify implementation; they do not prove it. OFF-path rendering identity still requires implementation review. Settings/preset counts are coherent.

**C1:** Deferral is justified; “near-worthless” is unproved (§6.2). Exact caching can also retain sphere classifications and reduce traversal overhead. C0 measures the opportunity’s ceiling, not cache profitability.

Codex session ID: 01a0f2d4-d92d-7d52-962e-434ab4eba5fc
Resume in Codex: codex resume 01a0f2d4-d92d-7d52-962e-434ab4eba5fc
