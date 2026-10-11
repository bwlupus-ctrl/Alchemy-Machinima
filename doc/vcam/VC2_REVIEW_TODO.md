# VC2 review follow-ups (Opus, review of 484e8585370: GO). Fix these in the VC3 round.

- MAJOR, llprismlens.cpp ~8358-8363: the surface-lens preview branch never calls `lens_camera.setFar`. Only the projection is capped to 64 m (~8318), so updateCull/stateSort cover the full draw distance. Apply the same far-clip as the camera-feed branch (~8274).
- MAJOR, tests/llprismpreviewpolicy_test.cpp:75-83: the stale-AO "sentinel" test asserts its own assignment. Make it exercise the real clear path: set a stale AO sentinel, run the preview clear/bind decision, and assert neutral.
- MINOR, ~3168-3190/2661-2680: no letterbox UVs (spec 4.1 item 2). Non-16:9 captures are stored anamorphically. Add letterbox UV regions, and fix the VC2_NOTES "no deviation" claim.
- MINOR, ~8126-8127 + pipeline.cpp:1830: the pinned pack is allocated outside ScopedPrismRenderState, so FBO/texture bindings change before the main render on warm-up frames. Allocate inside the scope; back off retries when allocation fails.
- MINOR, llprismpreview.cpp:24-44: setDemand(null handle) records demand. Reject null. The visibility table silently drops owners beyond 4: assert, or grow it.
- MINOR: a preview publication overwrites the retained output as CURRENT, so the world face can briefly show the 192x108 frame when the capture becomes visible. Mark preview publications as preview-quality so visible composites don't use them, or force a visible render first.
- MINOR: a near-zero float remainder gives previews a tiny entitlement plus an immediate first attempt. Add an epsilon floor.
