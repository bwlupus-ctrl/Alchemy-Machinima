# VCam Projector VP1 + VP2 implementation notes

## Scope implemented

- Added a projector registry separate from display bindings, capped at eight records. Object targets require a live, non-HUD `LLVOVolume` with light enabled and a projector texture; ownership and modify permissions are intentionally irrelevant because the binding is viewer-local.
- Added generation-bearing projector handles plus validate, bind, reassign, settings, Gate-subscription, remove, snapshot, health, and retained-feed APIs. `ALVCamPageModel` exposes the same mutation surface and creates no undo/history entries.
- Projector settings are pipeline-neutral: Fit/Fill/Stretch, anchor, feed EV, exposure policy, 256/512/1024 resolution, no-signal policy, optional gobo preservation, and `ScreenEffects`. `getProjectorFeed()` returns the retained capture target, logical UV region, publication generation, and settings; VP3/VP4 decide whether to convert or bind it.
- Configuration changes use the existing main-thread `ScopedRevisionSignals` path. Runtime health/demand changes use the existing coalesced/rate-limited runtime revision signal. Empty snapshots contain zero projector rows; offline targets report `OFFLINE`, remain quarantined, and revalidate to `READY` when their object resolves as a projector.
- Removing a capture removes its projector bindings. Display removal keeps the retained output alive when projector demand still owns it. OTS failure rollback also preserves projector identities/settings so the pre-existing atomic replacement guarantee remains intact.

## Scene compatibility

- Director scene data adds the optional top-level `prism_projectors` array. Each record stores ID/generation, capture/target identity, settings/effects, and Gate subscription. Absence means no projector bindings, so old scenes load identically.
- The current Prism parser validates only its required known arrays and ignores unknown top-level keys. Older builds therefore tolerate `prism_projectors`; the Director scene schema version was not bumped.
- Offline object IDs load as quarantined records. A live non-projector target is rejected atomically with an explicit projector-light reason.

## Demand behavior

- `setupSpotLight()` stamps consumption only for the main view, never Prism auxiliary or cube/probe rendering.
- A READY binding consumed within two frames, or within its 30-frame bind/load bootstrap, joins visible/Gate demand before preview fallback. A projector-only camera therefore uses its normal full-quality resolution/rate/adaptive budget rather than the pinned preview path.
- Removing/unbinding eliminates demand immediately; an unstamped binding ages out after the two-frame grace. With zero projector bindings, capture scheduling decisions are unchanged. The feature-off stamp path returns before registry work.

## Tests and verification

- Added `llprismprojector_test.cpp` and CMake registration for schema defaults, separate capacity/handle domains, empty-state behavior, bootstrap, offline suppression, and the two-frame consumption boundary.
- Per request, no configure, build, executable test, commit, fetch, or upstream operation was performed. Verification was limited to static symbol/call-site review, LLSD compatibility review, `git diff --check`, and LF checks.
- VP3 feed preparation/conversion and VP4 pipeline cookie binding are deliberately not implemented.
