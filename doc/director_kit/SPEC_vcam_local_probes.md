# VCam Output: Local Reflection Probes — plan for peer review (v3, 10/16)

Status: **plan; peer review round 3**. v2 review: Opus one P1 from green, Codex and Argon NOT GREEN with converging findings; the v3 changes are in §2b and override anything earlier that they contradict. Queued after the build/test, the themes and the speed ramp.
v1 (a hand-built "const" aux UBO builder) was reviewed by Opus, Codex xhigh and Gemini Argon. **All three said PLAN NOT GREEN**, for the same core reasons:
- probes must be depth-sorted for the shader's bucket scan, and distance ranking breaks it;
- `refIndex.x` must stay `mCubeIndex`, because the radiance and irradiance atlases share it;
- `getBox()` writes `mRadius` and reads the global modelview, so a "const" builder is impossible without duplicating the math;
- neighbour lists filter on main-eye `mOccluded`.

They also showed that the v1 premise was mostly false. The per-probe fields `updateUniforms` writes (`mProbeIndex`, `mMinDepth`/`mMaxDepth`) are rebuilt on every call, and cube-face probe renders already call `updateUniforms` from other cameras every frame (`pipeline.cpp:8010`, `llreflectionmapmanager.cpp:3274`). `mLastBindTime` is never read.

v2 therefore takes the simpler design that Opus proposed and the reviews support: **reuse the real `updateUniforms()` from the aux camera**.

## 1. Problem (unchanged)
The VCam feed uses only the sky/default probe (`activatePrismAuxiliaryProbeState`, `pipeline.cpp:11154`). Local probes and hero mirrors are excluded, so interiors look flatter or brighter in the feed than through the main camera.

## 2. Design
Inside the Prism aux scope, **after the aux camera and modelview are installed**:
1. Call `mReflectionMapManager.updateUniforms(/*aux=*/true)` from the aux camera. The real code handles everything the shader requires, unchanged:
   - selection and depth sort (`CompareProbeDepth`);
   - `refBucket`, sentinel and clamp;
   - `refIndex.x = mCubeIndex`;
   - neighbour packing (count-delimited, aligned offsets);
   - `refParams` (fade, z-near);
   - box math via `getBox()`.
2. **Aux flag, part 1 (`aux=true`): skip `mOccluded` filtering**, in selection AND neighbour packing (`getReflectionMaps` ~2584, neighbour check ~3371), because occlusion is from the main eye. This is the only behavioural difference. With `aux=false` the code path is byte-identical to today.
3. **Aux flag, part 2: sky fallback centred on the VCam.** The default probe's origin follows the main camera (`touch_default_probe`, ~188-197: origin + 64 m up, radius 4096, full weight inside 2048 m). For the aux build, use `aux_eye + (0, 0, 64)` for slot 0's sphere. This is a local override inside `updateUniforms(aux)`; the `mDefaultProbe->mOrigin` member is not written. A VCam more than 2 km from you therefore keeps its sky.
4. Set `heroProbeCount = 0` in the staged copy, and keep the existing `prism_auxiliary` shader guards and suppressed hero binding (`reflectionProbeF.glsl:802/815`). Those, not the count, are what actually exclude mirrors.
5. Upload the result for the aux scope. At `endPrismAuxiliaryState()`, copy `mPrismSavedProbeData` back into `mProbeData` and re-upload, as today. The main pass then rebuilds probe and hero state at `pipeline.cpp:8010-8011` before the main geometry draws, so **the main view stays byte-identical**.
6. **Remove the "ProbeBeforePresent" stable-scale hack for the aux.** With `gCubeSnapshot` false here, `updateUniforms` produces the converged `ambscale = radscale = mResetFade` on its own, for every slot. The v1 hack only fixed slot 0.
7. **Re-upload only when it was used (Argon).** `endPrismAuxiliaryState()` currently re-uploads the 20 KB `mProbeData` on every `ScopedPrismRenderState` exit, including the projector-feed and preview-atlas scopes that never activate probes (`llprismlens.cpp:9751/9788`). Add `mPrismAuxProbeStateActive`, set in `activatePrismAuxiliaryProbeState()`, and restore only when it is set. This is a free saving.
8. **Fast path.** If `RenderReflectionProbeLevel == 0` (Coverage: None) or `PrismAuxLocalProbes` is off, keep today's default-only path.

**Side effects accepted**, because they are exactly what the main path already does every frame with other cameras:
- `updateUniforms` re-derives `mProbeIndex` / `mMinDepth` / `mMaxDepth` for the aux camera;
- it refreshes `mOrigin` / `mRadius` from live viewer objects (idempotent within a frame);
- it writes the hero UBO.

All of these are rebuilt by the main pass before use.

## 2b. v3 changes (from the v2 reviews: Opus, Codex xhigh, Argon low)
- **Slot 0 is always the sky probe.** The sort includes index 0, and the default probe's `mMinDepth` came from `mOrigin` (the main camera). With `aux=true`, `getReflectionMaps` computes the default probe's depth from `aux_eye + (0,0,64)`, keyed on `refmap == mDefaultProbe` (never on slot index), with `llassert(maps[0] == mDefaultProbe)`. `updateUniforms(aux)` uses the same override for refSphere[0]/refParams[0].
- **The fast path keeps today's protections.** The default-only path is used when `PrismAuxLocalProbes` is off, when Probe Coverage is None, for preview-only captures (`sPrismPreviewRender`), and **when the default probe is not complete** (startup or reset). It keeps the existing ProbeBeforePresent stable-scale fix AND gains the aux-eye centring. Switching the setting off, or starting up, therefore never flickers or loses the sky. §2 item 6 (removing the hack) applies only to the full `updateUniforms(aux)` path.
- **No stale indices.** In `getReflectionMaps(aux)`, set `mProbeIndex = -1` for every resident probe that is not selected (e.g. incomplete). Neighbour packing with `aux=true` also requires `neighbor->mComplete`, so an incomplete probe can never be sampled through a neighbour list. This is aux-only, so the main path stays byte-identical.
- **Isolation for byte-identity and scheduling.** Before the aux call, snapshot `mOrigin`, `mRadius`, `mProbeIndex`, `mMinDepth`, `mMaxDepth` for the resident probes (at most 256, into a pipeline-owned fixed array), and restore them right after. The main view's sort, buckets and probe scheduling then see exactly what they would have without the VCam, including for moving or scaling probes.
- **Hero and GL state.** With `aux=true`, `updateUniforms` zeroes `heroProbeCount` in `mProbeData` BEFORE its upload, skips `mHeroProbeManager.updateUniforms()`, and restores the previous `GL_UNIFORM_BUFFER` binding. That means one upload, not two.
- **§2 item 7 replaced.** The restore is already gated by `mPrismProbeUBOStaged` (`pipeline.cpp:11211/11262/11280`), so no new flag is added. The real saving is moving the 20 KB `mPrismSavedProbeData` copy (and its GL query) out of `beginPrismAuxiliaryState()` into `activatePrismAuxiliaryProbeState()`, so the projector-feed and preview-atlas scopes stop paying for it. The aux build's own upload is counted in the cost.
- **Moving manual box probes.** Moving the origin/radius sync ahead of the depth computation is NOT done, because it would change the main view. The aux uses the same order as main, and the documented one-frame staleness stands.
- **Slot 0 is PINNED, not sorted (Codex v3 P1).** With `aux=true`, `getReflectionMaps` places `mDefaultProbe` at index 0 explicitly and depth-sorts only the selected locals into 1..N-1 (`std::sort(maps.begin()+1, ...)`). A local probe far *behind* the VCam (minDepth ≈ −5000) can therefore never displace the sky probe. The slot-0 depth override still sets its `refSphere`/`refParams`; the `llassert(maps[0] == mDefaultProbe)` becomes a guarantee, not a detector. Main path unchanged.
- **Startup / incomplete default (Codex v3 P1).** When `mDefaultProbe` is not `mComplete`, the aux uses the fast path, defined explicitly as follows:
  - slot 0 = `mDefaultProbe`'s cube index 0, with `refmapCount = 1`;
  - `refIndex[0]` = {0, −1, 0, 0}, i.e. no neighbours;
  - the sphere is centred on the aux eye (radius 4096);
  - the fade comes from the default probe's own fade value;
  - stable scales;
  - `heroProbeCount = 0`.

  This is exactly the state the main view samples at startup (atlas slot 0), so the feed is never worse than the main view. It never copies a stale slot-0 index or fade from `mProbeData`.
- **Exact sequence (Opus v3 review):**
  1. `beginPrismAuxiliaryState()` keeps only the `mUBO == 0` lazy init. It runs under the main camera, so it must stay here; never in activate, which runs under the aux camera.
  2. `activatePrismAuxiliaryProbeState()` saves `mPrismSavedProbeData = mProbeData` and sets `mPrismSavedProbeDataValid`.
  3. Snapshot the probe fields.
  4. Run `updateUniforms(aux)`: build and upload once.
  5. Restore the probe fields. They don't feed the already-uploaded buffer, so restoring after the upload is safe.
  6. Set `mPrismProbeUBOStaged`.
  7. `endPrismAuxiliaryState()`, gated on `mPrismProbeUBOStaged`: copy the saved data back into `mProbeData` and re-upload.
- **Assert no early bind.** `mPrismSavedProbeDataValid` now becomes true only at activate, but it also gates `setUniforms`/`setEnvMat` for `prism_auxiliary` binds (`pipeline.cpp:28359`). Assert that no probe bind happens between `sPrismLensRender=true` (`llprismlens.cpp:9860`) and activate (`:10077`).
- **Selftest field compare:** compare `mOrigin`, `mRadius`, `mProbeIndex` and min/max depth. Ignore `mLastBindTime`, which is write-only and differs by design.
- **Tests.** Byte-identity is an in-viewer selftest with real probes: compare the main UBO bytes and the probe fields with the VCam on vs off, and with the setting off vs on. Startup case: with the default probe incomplete, the fast path is used and the sky is kept. Assert `maps[0] == mDefaultProbe` for the aux build.

## 3. Limits (documented, not fixed in v1)
- **Residency.** Only probes resident for the main camera are available, including the cinematic live-probe residency pin (`llreflectionmapmanager.cpp:465`). A VCam far from loaded probes gets the sky only.
- **Refresh.** A resident probe occluded from the main camera is not re-rendered (it is deferred or only auto-adjusted, ~558-584/911-916), so it can be stale in the feed when lights or avatars change. Making VCam visibility un-defer refresh is out of scope for v1.
- **Water.** Water in the feed will start sampling manual local probes, as the main view already does (`reflectionProbeF.glsl:815`). That is a visible, correct change.
- **Moving manual probes** use `mOrigin` from the latest refresh, which is at most one frame stale.

## 4. Setting and UI
- `PrismAuxLocalProbes` (Boolean, default 1; 0 gives today's sky-only feed).
- VCam kit page, Output card, row "Local reflections", help text: "Use loaded reflection probes in the feed; far from you the feed falls back to the sky." Mockup data first; the label must fit; add it to the wiring audit and search keywords (reflections, probes, local, interior).
- Feed status line: "Probes: N local" or "Probes: sky only".

## 5. Cost
- One extra `updateUniforms()` per VCam **capture**, at the capture rate rather than necessarily every frame. That includes selection, sort, bucket fill and neighbour packing for up to 256 resident probes, comparable to one main-view call.
- The stage and restore uploads already happen today (and with item 7 they happen *less* often).
- The GPU does the same per-pixel probe sampling in the feed that the main view already does.
- Tracy zone "PrismAuxProbeUniforms" measures it.

## 6. Tests
- **Byte-identity.** `updateUniforms(false)` produces exactly today's output: a unit/selftest compares the UBO bytes before and after the change on a fixed probe set. The main-view UBO is unchanged with the VCam on or off.
- **Aux build.**
  - Occluded-from-main probes are included.
  - Slot 0 is centred on the aux eye.
  - The sort and buckets are valid. Assert the invariants: ascending minDepth for 1..N-1, and bucket starts ≤ first influencing index.
  - `refIndex.x == mCubeIndex`.
- **Scope.** The restore runs only when probes were activated (item 7).
- **In viewer (after build).** An interior with a placed probe shows local reflections in the feed when the VCam is near, and the sky only when it is far; the status text matches. A VCam 3 km away still has sky reflections. Record the Tracy cost per capture.
- **Gates.** zs_check, the kit gates, label fit, the wiring audit, then Opus + Codex (xhigh) + Argon (low), then a build, then a screenshot compare of the feed vs the main view at the same spot.

## 7. Non-goals
Rendering probes for far VCams; hero/mirror probes in the feed; un-deferring refresh for VCam-visible probes; per-VCam probe budgets.

## 8. Effort
About half a day: an aux flag through `getReflectionMaps`/`updateUniforms`, the hook and restore changes in `pipeline.cpp`, one kit row, and the tests. Implementer: Codex (high).
