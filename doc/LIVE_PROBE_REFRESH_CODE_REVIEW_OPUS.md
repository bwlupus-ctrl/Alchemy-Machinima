# Live Probe Refresh — adversarial code review (Opus, independent of Codex)

Scope: `git diff c221cda3732` (11 files) plus untracked `alcineliveproberefresh.h`. Read-only; nothing compiled.

## Verdict: NO P0. One P1 (design claim is false). Six P2.

## Compile-safety pass (no defects found)
- **Header**: every function is `inline`, all in `namespace ALCineLiveProbeRefresh`, include-guarded. `stdtypes.h` supplies F64/U64 and `lluuid.h` supplies `UUID_BYTES`/`mData`. `<algorithm>` covers `std::clamp` (both arguments `int`), `<cmath>` covers `isfinite`, `<cstring>` covers `memcpy`. Overloads are unambiguous: `addExact(U64 / const LLUUID& / bool)`, and every call site passes an exact type.
- **Macro collisions**: I grepped the enumerators (ABS, REL, NONE, FULL, IDLE, COLOR, …) against `indra/`, the Windows SDK 26100 `um`/`shared` headers and `vcpkg_installed`. The only object-like hit is `NONE` in `ntddpar.h` and in webrtc-internal ICU. Neither is reachable from viewer TUs. `ABS(x)` is function-like, so `Tol::ABS,` cannot expand.
- **Const-ness**: `getPositionAgent`, `getScale`/`getRotation` (LLXform), `getLightSRGBColor`, `getSpotLightParams`, `getLightFalloff`, `getIsLight`, `isDead` and `isProjectorNoShadow` (static) are all const or static. Every `.mV` pointer taken from a temporary is used within its full expression, and the callee copies it.
- **Setting types**: the Color4, S32 and Boolean types match settings*.xml, so no `LLCachedControl` type-mismatch abort.
- **`LL_PROFILE_ZONE_NUM(faces)`**: `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY` maps to `ZoneNamedN(___tracy_scoped_zone…)` because DISPLAY=1, so `ZoneValue` resolves. The same pattern is at lldrawpool.cpp:1263.
- **TUT**: `test_group<Data, 72>` is valid (tut.hpp:130). test<55..69> sit under the ceiling. The anonymous namespace sits inside `tut`, so `fail(std::string)` and `ensure` resolve. Nothing needs C4101-level attention.
- **CMake**: adding the header to the list is enough.

## Moved code
`updateRealtimeProbeAllFaces` is token-identical to HEAD :497-541: same zone, same order, same flags. In Every-frame mode and the fallback path the only extra work is cached reads plus a guarded no-op reset. **Confirmed identical.**

## Scheduler vs spec
The following all match R3 and are covered by M3–M14, and I re-derived the M9 and M11 frame counts:
- A→B→A dirtying
- alternation under continuous change (radiance gap ≤6)
- watchdog pair / second-half reason
- settle boundary
- identity/mode reset
- stop-at-pass-end

Scratch isolation is sound: budget faces go to `count+1` and the round-robin goes to `count` (:1288-1293). `mRadiancePass` is set per face and restored. The frozen origin is restored before `updateNeighbors`.

## P1
**P1-1: the claim "H of everything that changes what the live probe would capture" (llreflectionmapmanager.cpp:~726) is false.** The capture's own light loop (pipeline.cpp:9568-9625) includes:
- **all non-rig local lights**: world prims, plus avatar *attachment* lights, which render even though the avatar is hidden;
- scaled by `AlchemyGlobalLightScale` (pipeline.cpp:9553);
- gated by `RenderAttachedLights` and the bdmerge light filters.

None of these is in H. In the **default** mode, a scripted light switching, a streetlight coming on or a face light moving therefore stays wrong for up to WatchdogSec (5 s). HEAD fixes the same change in one frame. This is a visible default-mode regression, not a tuning issue.
- Fix options:
  - add a coarse signature of the lights the capture would pick (for example, `mLights` within the probe radius plus the light range: ID, position ABS move_m, linear colour COLOR, radius);
  - add those three settings;
  - or explicitly accept the gap in the spec and tooltip.
- Needs a decision, not a guess.

## P2
1. **Target-rig omnis and catchlight are ignored by the capture** (`liveProbeIgnoredLightIds`, alcinelightrig.cpp:1110). They are still hashed, and their flicker still sets `liveProbeAnimating` (alcinelightrig.cpp:1202). A flickering target omni therefore forces Every-frame cost permanently, and tweaking it triggers pointless refreshes. Skip the target slot's omnis and catchlight in both places.
2. **`shift()` (:1515) moves `mOrigin` but not `mCineFrozenOrigin`.** If an origin shift lands mid-pass, the remaining 1–5 faces are captured about a region-width away and published once. It is probably masked by `destroyLiveProbe` on region change. Reset or shift the frozen origin there.
3. **FULL after BUDGET uses `mRealtimeRadiancePass`** (:537), which budget passes never update. The same kind can therefore run twice, and convergence is delayed by one frame. Harmless.
4. **Manual pressed after an irradiance-ended pass**: the radiance half starts first, then `mIrrOk=false` makes the next pass `CHANGED` (header :541). The status mislabels it. The request is still honoured.
5. **Log honesty**:
   - resets inside `decide()` (identity/mode) are not counted in `resets`;
   - clean/dirty count only budget passes, while pubs include FULL frames;
   - after a gap off the scheduler branch, the "per-second" window can span many seconds of raw counts.
   Label the counts as a window, or normalise them.
6. **"Refresh now" latches `mManual` in Every frame / Balanced / Economy.** It is never consumed there and is later wiped by the reset. The button should be disabled outside On change, which the tooltip already implies.

## UI
- The combo (62–232), button (240–332) and 68 px status box (138–206, card 214) fit.
- Worst-case status text is 4 lines: state, refresh line of ≤45 characters, and a Fill line that wraps to 2. 68 px holds it with little margin.
- The combo uses string values with an S32 control, which is the standard viewer pattern.

## Hash cost
The vectors keep their capacity, so there is no per-frame allocation. The estimate is about 1k floats and 450 fields for 5 rigs × 9 emitters, with byte-wise FNV, giving roughly 10 µs (INFERENCE). The `hash_us` counter will measure it.
