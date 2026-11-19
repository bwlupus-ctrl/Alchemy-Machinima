# Adversarial review R3 (Opus): PROBE_UPDATE_ON_DEMAND_BRIEF_V4 (+V3)

Read-only review at HEAD 303065bda35. Every anchor was re-read. User rule: anything that breaks the promise is a P0.

**Verdict: GO-after-fixes.** U1 and U2 are the right calls and most of V4 holds. But the U1 implementation as written **silently removes every local light from ON-path ordinary captures** (P0-1). There is also a use-after-free in the texture fan-out (P0-2), and six more promise-breakers. All have local fixes. No redesign is needed.

## 1. R2 items against V4 (checked in code)

| R2 | Status |
|---|---|
| N1 continuous movers/lights | **Fixed in principle** (§4.10). Edge cases are open: P0-4, P0-5, P0-6. |
| N2 mesh/sculpt LOD arrival | **Fixed for LOD.** It opens a hole for real asset swaps (P0-7). |
| N3 fade bucket / cap | **Fade removed.** The global over-cap rule counts every `mLights` entry in every loaded region (P1-1). |
| N4 gobo arrival | **Fixed as a design.** The fan-out as written can dereference freed pointers (P0-2). |
| N5 re-sharpen | **Over-corrected.** The global face counter makes it camera-driven (P0-8). |
| P1-a shift | **Fixed.** Offsetting sticky fields 0-2 plus the streak bounds is consistent. |
| P1-b / P1-c / P1-d | **Fixed.** `CineRigRimTint` is F32 at alcinerigrim.cpp:218; `Slot` casts are specified; the large-texture event bumps Live. |
| Tests / UNSETTLED | **Added** (T23-T36, §6.4). More are needed (§5). |

**llTargetOmega offscreen question (the author's least-sure item):** it is **not** camera-gated. PROVES:
- `LLViewerObjectList::update` calls `idleUpdate` for every entry of `mActiveObjects`, with no visibility test (llviewerobjectlist.cpp:956-1006).
- `applyAngularVelocity` runs under `!mStatic && sVelocityInterpolate && !isSelected()` (llviewerobject.cpp:2520-2538).
- `updateMovedList` has no visibility gate (pipeline.cpp:3550-3589).
- Texture animation is the same: `LLViewerTextureAnim::updateClass` walks every instance (llviewertextureanim.cpp:80-85).

So spinners stop only on selection, FreezeTime, VelocityInterpolate off, or Temporal-Capture 0×. None of those is camera motion, so settle events are not camera-driven. T23b is expected to PASS.

## 2. P0 (promise-breakers)

**P0-1. Probe-centric lights are empty in the deferred pass (§4.5.1).**
- In a cube face, the deferred local-light loop runs **before** the list is rebuilt:
  - `renderDeferredLighting` iterates `mNearbyLights` at pipeline.cpp:~23263.
  - It calls `renderGeomPostDeferred` only at :23635.
  - That is the **only** `calcNearbyLights` call site (:7518).
- V4 wraps each `probe->update` in `begin` (save + **clear**), so every ON-path face lights opaque geometry with an **empty** list. The transient list is built afterwards and serves only forward alpha, and then `end` restores the main-eye list.
- Result: ON ordinary, default and sliced probes contain no local lights at all.
- **Fix:** build the list before the lighting pass. Inside the scope, in `display_cube_face` right after `display_update_camera()` (llviewerdisplay.cpp:1290), call `gPipeline.calcNearbyLights(*LLViewerCamera::getInstance())`, gated on `isProbeCentricLightCapture()`. It is public (pipeline.h:745). Add llviewerdisplay.cpp to the allowed edits for this line.
- **Pre-existing, shipped (report to the user; do not silently fix):** Live's scopes (:648-649, :704) have the same ordering.
  - Face 0 of every Live batch renders with an empty list.
  - Faces 1-5 use the previous face's frustum-filtered list.
  - In Economy / 1-face budget, every Live face has no local lights.
  - PROVES by call order. Confirm in-world with a Live probe near a rig in Economy vs Every frame.

**P0-2. Use-after-free in the H6 fan-out.**
- `getFaceList(ch)` and `getVolumeList(ch)` (llviewertexture.h:176, :181) return vectors whose tail beyond `mNumFaces` / `mNumVolumes` holds **stale pointers**: `removeFace` / `removeVolume` swap-and-decrement without erasing (llviewertexture.cpp:1452-1458, :1505-1511).
- **Fix:** iterate `i < getNumFaces(ch)` and `i < getNumVolumes(LIGHT_TEX)`, as the sculpt loop does at :2135.

**P0-3. Refresh-all can hang or declare done falsely (§4.11).**
- *Hangs:*
  - Allocated probes with `!isRelevant()` are skipped before selection (llreflectionmapmanager.cpp:426-430; `isRelevant` at llreflectionmap.cpp:292-312 is false for automatic probes at levels 1/2). The slot-allocation loop still gives them cubes, so they get `mBarrier` and are never captured.
  - A designated Live probe that has no cube or is irrelevant never becomes `cinematicLive`, so `mLivePending` never clears.
  - EEP transitions call `pause()` (llenvironment.cpp:2943, :3546), which stalls every non-default probe.
- *False done:* Live "done" counts `mIrrPub`/`mRadPub` deltas. A budget pass that **started before arming** and ends after it is counted.
- **Fix:**
  - Barrier only relevant, allocated ORDINARY probes. Re-check eligibility each frame.
  - Treat a Live probe without a cube or relevance as `Live: unavailable`, never "done".
  - Count only Live passes whose face 0 ran after arming.
  - Show `paused` while `mPaused`.
  - Add a timeout (e.g. 30 s) that reports `incomplete k/n`, and never "All probes refreshed".

**P0-4. Slow periodic sources never settle (§4.10).**
- The fixed 0.25 s gap misses sources with a period of 0.3-5 s: scripted texture slideshows, colour cycles, `llSetPos`/`llSetRot` timer motion, server-paced movers.
- Each event re-dirties, and the probe starts in every 5 s window, so it is UNSETTLED.
- The same fixed gap also breaks at low fps. Machinima high-res or offline capture below 4 fps never forms a streak.
- **Fix:** adaptive gap.
  - Use `gap_max = max(0.25 s, 3 × frame interval)` for the streak.
  - Add a slow tier: 4 or more events from one key within 10 s → suppressed.
  - Settle after quiet ≥ `clamp(2 × median gap, 0.5, 5) s`.

**P0-5. A real edit on a streaming key is hidden.**
- Streaks key only by pointer. A retexture or recolour of a spinner, or a move or retarget of a flickering light, is merged into the suppressed union and emitted only at settle, which is never for a perpetual spinner or flicker.
- **Fix:** streak per (key, motion class).
  - Only transform events (H1a/H1b) and animated light fields (colour/intensity) join streaks.
  - `R_TEX` / H4-content / H2 events, and structural light fields (radius, spot, texture, gobo, no-shadow), bypass suppression.
  - Keep the light position in the motion hash.

**P0-6. Overflow ordering makes suppression moot.**
- The recorder cap of 4096 coalesced keys is applied **before** streak filtering. §4.6 step a checks E×P before step b filters.
- TXa emits every frame for every texture-anim instance, independent of the camera, and a 4-9 region draw set can exceed 4096 animated or omega keys. Result: overflow every frame, then R_RESYNC all plus a Live bump every frame, so nothing ever settles and Live never idles.
- **Fix:**
  - Keep suppressed keys in a recorder-side set that is checked at note time. Suppressed notes update the streak only and never take a slot.
  - Compute E×P **after** filtering, and state that order.

**P0-7. A real mesh/sculpt asset swap is hidden by the MS tag (§4.4 MS/H4).**
- `mProbeBuilt` survives a change of sculpt/mesh id. The replacement mesh's arrival goes through `notifyMeshLoaded` (llvovolume.cpp:1284-1287), which is tagged as LOD, so the probe keeps the old or placeholder geometry.
- **Fix:** store the built identity (sculpt id + type). Tag an arrival only if the identity is unchanged. Clear `mProbeBuilt` when volume params change.

**P0-8. The DS/re-sharpen stamp is camera-driven (§4.4 H6 (c)).**
- `probeFaceCounter()` is global. Any safety or other face anywhere means every re-sharpen after a VRAM downscale notifies, and those downscales come from main-camera virtual size.
- Under VRAM pressure, T7-strict (`t = 0`) fails on every orbit.
- **Fix:** give the event a `mMinStartSerial` equal to the serial at blur time. It hits only probes whose last txn start serial ≥ that value, i.e. those that really captured the blurred texture.

## 3. P1

- **P1-1. Over-cap rule and light-diff cost (§4.5.3).**
  - The eligible count is taken over all `mLights`, across every loaded region. Past 256 lights sim-wide, every light event and settle becomes global. A crowd with facelights stopping and starting then refreshes all probes.
  - The diff builds a ~45-field Signature per light per frame. That could cost ms at thousands of lights.
  - **Fix:**
    - Keep a per-probe local eligible count (lights within `min(RenderFarClip, far) + r·1.5` of the probe; refresh it at ≤ 1 Hz or on light events). Globalise only for over-cap probes.
    - Prefilter lights to the union bound of the ORDINARY probes and Live.
    - Reuse Signature buffers.
- **P1-2. Camera state in the light filter.** `isTooSlow()` is AutoTune-driven, from render time, and can flip with camera view. It is the same filter the transient branch uses, so it is real, but T7 fails when AutoTune is on. Document it or exclude it under ON.
- **P1-3. S13 is not independent.** The expected table is transcribed by the same implementer. Also require the moved-code diff to be reviewed line-for-line against HEAD :777-892, and a golden H literal computed once from a HEAD build's `[LiveProbe]` hash on a fixed synthetic sample.

## 4. P2

- The first frame of slicing can be selected by ordinary (the owner flips only after a sliced frame). This converges; document it.
- H7 is largely redundant: `markForUpdate` and `rebuildMaterial` already reach H3. It is harmless.
- The per-face light scope must be RAII over the whole `probe->update`, including `LLReflectionMap::update`'s early return. As specified, it is.
- The pinned count forced to 0 outside Live, and the ignored/pinned predicates (llreflectionmapmanager.cpp:1197-1212), are correct. No main-eye, Prism, Hero or shadow leak: fade clocks are untouched (transient entries start at `LIGHT_FADE_TIME`), and spot-shadow targets are `!gCubeSnapshot`-gated.
- The OFF path stays byte-identical: `isProbeCentricLightCapture()` is false, so :9726 behaves as HEAD.

## 5. Residue

- **Acceptable backstops, with disclosure:** flexi, media frames and water waves (frozen, refreshed by MaxAge 10-600 s and Refresh-all).
- **Over-dirty only, but fix P1-1:** over-cap lights.
- **Blockers until fixed:** slow sources (P0-4).

## 6. Budget, compile, tests

- **Budget:** still ≤ 7 faces per `update()`. The barrier and slicing add none.
- **Compile:**
  - `getNumFaces` / `getNumVolumes` (P0-2).
  - `bdmerge_should_render_light` is file-static, so it needs the wrapper.
  - `Event::mKey` / `mMinStartSerial` must be added to the pure header.
  - The Signature must be reused per light, not constructed per light.
- **Tests:** T0-T36 are insufficient. Add:
  - **T37** ON capture shows a bright point light that sits next to a probe (catches P0-1);
  - **T38** Live Economy near a rig vs Every frame (pre-existing);
  - **T39** swap a mesh asset on a built object;
  - **T40** a 1 s texture slideshow and a 0.5 s `llSetRot` timer (settle);
  - **T41** a spinner at ≤ 3 fps capture;
  - **T42** recolour a spinning prim, and move a flickering light, with the edit shown within the latency model;
  - **T43** Refresh-all at coverage level 1/2, with a Live probe without a cube, and during an EEP transition (no hang, no false done);
  - **T44** T7-strict under VRAM pressure (`t = 0`);
  - **T45** 5000 or more texture-anim/omega keys in view distance (no overflow storm);
  - **T46** more than 256 lights sim-wide, with a crowd stopping and starting (no global storm).
