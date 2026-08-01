# Preferences Compatibility / Performance Tiers — Deep-Research + Codegen Brief

**Fork:** Alchemy-Machinima (`I:\alchemy-machinima`, `develop`).
**Type:** research + no-build codegen brief. Produce (a) a research writeup and (b) a git-applicable patch. **Do NOT build.** After delivery: Codex + Opus + Fable adversarial review, then Claude builds.
**Grounding:** this brief consolidates three independent code-grounded audits (two Claude Explore passes + one Codex consult). Every value/anchor below was read from the current tree. Re-verify anchors as you code; line numbers are approximate.

---

## 0. Goal + confirmed decisions

Let users on **weaker machines than the author's** (author = RTX 5090 / Ryzen 9950X / 192 GB / ReShade) run this machinima fork well. **Target = mid-range and broader; explicitly NOT low-end/potato.** Users may have a mid dGPU or strong Intel iGPU, **16–32 GB system RAM**, and **no ReShade**.

Confirmed with the user:
- **Four tiers**, each based on an existing stock quality level, with fork/memory overlays on top:
  - **Flagship** → base Ultra (6) — *exactly today's behavior, unchanged.*
  - **High** → base HighUltra (5) — strong dGPU, 32 GB.
  - **Balanced** → base High (4) — mid dGPU, 16–32 GB. **← the target.**
  - **Modest** → base MidHigh (3) — entry dGPU / strong iGPU, 16 GB.
- **Full first slice:** a new Preferences **Compatibility** tab + tier picker + Apply + all four bundles + grouped individual knobs (GPU / Memory / ReShade) + the two new code gates + Cancel snapshot/restore + restart-required banner.

**Prime directive:** with the default tier (**Flagship**) and no user action, behavior + performance are **byte-for-byte what they are today**. Nothing is auto-applied on install/upgrade. Every tier change is reversible.

---

## 1. Cost reality (what a weak machine actually pays for)

Three audits converged: **the fork's heavy render features are default-OFF and verified to early-out (and release their VRAM) when off** — froxel volumetrics, projector volumetrics, godrays, velocity/motion-blur, soft/Vogel shadows, visible-diffuse sidecar, Ghost-Studio deferred clones, weather, SSR. A fresh install pays ~nothing for them. **SSS and area lights do not exist in-tree — do not add settings for them.**

The costs that DO hit a weaker machine:

**Fork-added, always-on at defaults:**
- **HDR + bloom** (`RenderHDREnabled=1`): HDR G-buffer formats + exposure/luminance + tonemap every frame, plus a **full-resolution five-level bloom pyramid**. This is the *only material fork-added default GPU cost*. Alloc `pipeline.cpp:~1081/1190`; bloom extract/downsample/upsample `pipeline.cpp:~11037`; HDR default `settings.xml:~22576`. **Note:** the main scene target is already `GL_RGBA16F` even with HDR off (`pipeline.cpp:~1141`), so disabling HDR removes bloom/exposure/tonemap but not that buffer.
- **Decoded-texture RAM pool + decoded-mesh RAM pool** — the real weak-machine risk (see §3). Up to **50% + 10% = 60% of system RAM**, currently uncapped.
- Minor: gobo anisotropic (`BDMergeGoboAnisotropic`), dithering (`RenderDitherEnabled`), 10-bit swapchain (`RenderGLContext10bitSDR`, negligible — R10G10B10A2 is still 32bpp), ReShade bridge per-frame gather (CPU-only; §4).

**Stock/Alchemy baseline levers a tier must also control (these outweigh most of the above):** shadow detail + SSAO (`pipeline.cpp:~15176`), reflection probes / PBR, transparent-water scene copies (`lldrawpoolwater.cpp:~97/186`), CAS + AA (`pipeline.cpp:~14593/14605`).

**Top 5 a mid-range user feels:** (1) shadow detail 2 + SSAO, (2) reflection probes + draw distance + local-light count, (3) full-res 5-level HDR bloom, (4) transparent-water copies, (5) AA + CAS.

---

## 2. Tier bundles — EXACT `gSavedSettings` values

Base levels are the stock masks in `llfeaturemanager.cpp:~186` (`sGraphicsLevelNames` Low..Ultra = 0..6), defined in `featuretable.txt:~93–335`. `applyCompatibilityTier()` first calls `setGraphicsLevel(baseLevel, /*skipFeatures*/true)` then stamps the overlay values below with normal `gSavedSettings` setters.

### 2a. GPU / visual overlay

| gSavedSettings | Flagship | High | Balanced | Modest |
|---|---:|---:|---:|---:|
| `RenderQualityPerformance` | 6 | 5 | 4 | 3 |
| `DebugQualityPerformance` | 6 | 5 | 4 | 3 |
| `RenderFarClip` | 256 | 192 | 128 | 96 |
| `RenderVolumeLODFactor` | 2.0 | 1.75 | 1.5 | 1.375 |
| `RenderShadowDetail` | 2 | 2 | 1 | 0 |
| `RenderDeferredSSAO` | 1 | 1 | 1 | 0 |
| `RenderFSAAType` | 2 | 2 | 2 | 1 |
| `RenderFSAASamples` | 3 | 2 | 1 | 0 |
| `RenderDepthOfField` | 0 | 0 | 0 | 0 |
| `RenderCASSharpness` | 0.40 | 0.30 | 0.00 | 0.00 |
| `RenderAnisotropicLevel` | 8 | 8 | 4 | 2 |
| `RenderReflectionsEnabled` | 1 | 1 | 1 | 1 |
| `RenderReflectionProbeLevel` | 3 | 3 | 2 | 1 |
| `RenderReflectionProbeDetail` | 2 | 1 | 0 | 0 |
| `RenderReflectionProbeCount` | 256 | 128 | 64 | 32 |
| `RenderReflectionProbeResolution` | 128 | 128 | 64 | 64 |
| `RenderScreenSpaceReflections` | 0 | 0 | 0 | 0 |
| `RenderTransparentWater` | 1 | 1 | 1 | 0 |
| `RenderLocalLightCount` | 8192 | 4096 | 2048 | 512 |
| `RenderMaxPartCount` | 8192 | 4096 | 4096 | 2048 |
| `RenderMaxTextureResolution` | 2048 | 2048 | 2048 | 1024 |
| `RenderAvatarMaxNonImpostors` | 16 | 12 | 8 | 6 |
| `RenderAvatarMaxComplexity` | 350000 | 300000 | 200000 | 100000 |
| `RenderAvatarLODFactor` | 1.0 | 1.0 | 1.0 | 1.0 |
| `RenderTerrainLODFactor` | 2.0 | 2.0 | 2.0 | 2.0 |
| `RenderTreeLODFactor` | 1.0 | 0.5 | 0.5 | 0.5 |
| `RenderTerrainPBRPlanarSampleCount` | 3 | 3 | 1 | 1 |
| `RenderGlowResolutionPow` | 9 | 9 | 9 | 8 |
| `RenderResolutionDivisor` | 1 | 1 | 1 | 1 |

> Do NOT expose `RenderWaterRefResolution`/`RenderWaterMaterials` as compat controls — defined but no effective consumers in-tree. `RenderTransparentWater` is the real water-cost switch.

### 2b. Fork / HDR overlay

| gSavedSettings | Flagship | High | Balanced | Modest |
|---|---:|---:|---:|---:|
| `RenderHDREnabled` | 1 | 1 | 1 | 0 |
| `RenderBloomHDR` | 1 | 1 | 1 | 0 |
| `RenderBloomMipCount` | 5 | 5 | 4 | 3 |
| `RenderBloomResolutionScale` | 0 | 1 | 2 | 3 |
| `RenderBloomHalation` | 0 | 0 | 0 | 0 |
| `AlchemyRenderTonemapType` | 1 | 1 | 1 | 1 |
| `RenderDitherEnabled` | 1 | 1 | 1 | 1 |
| `RenderGLContext10bitSDR` | 1 | 0 | 0 | 0 |
| `BDMergeGoboAnisotropic` | 1 | 1 | 0 | 0 |
| `BDMergeSoftProjectorShadows` | 0 | 0 | 0 | 0 |
| `RenderShadowSoftVogel` | 0 | 0 | 0 | 0 |
| `RenderVolumetricLighting` | 0 | 0 | 0 | 0 |
| `BDMergeProjectorVolumetrics` | 0 | 0 | 0 | 0 |
| `BDMergeFroxelVolumetrics` | 0 | 0 | 0 | 0 |
| `BDMergeVelocityBuffer` | 0 | 0 | 0 | 0 |
| `BDMergeMotionBlur` | 0 | 0 | 0 | 0 |
| `RenderVisibleDiffuseSidecar` | 0 | 0 | 0 | 0 |
| `BDMergeMaxSpotShadows` | 2 | 2 | 2 | 2 |
| BDMerge shadow-resolution overrides (all) | 0 | 0 | 0 | 0 |
| `RenderReShadeBridgeEnabled` (NEW, §5) | 1 | 0 | 0 | 0 |

> `RenderBloomMipCount`/`RenderBloomResolutionScale`/`RenderBloomHalation` become effective only once `RenderBloomHDR` is wired (§5.1); verify these control names against `settings_alchemy.xml` and adjust to the actual bloom controls if they differ.

### 2c. Memory / VRAM / cache overlay (the 16–32 GB core)

| gSavedSettings | Flagship | High | Balanced | Modest | Takes effect |
|---|---:|---:|---:|---:|---|
| `RenderMaxVRAMBudget` (MB) | 0 auto | 12288 | 8192 | 4096 | live |
| `RenderTextureVRAMDivisor` | 1 | 1 | 1 | 1 | live |
| `CacheSize` (MB) | 32768 | 24576 | 16384 | 8192 | **restart** |
| `MaxHeapSize64` (GB) | 0 auto | 28 | 14 | 12 | **restart** |
| `BDMergeTexPoolEnable` | 1 | 1 | 1 | 1 | live |
| `BDMergeTexPoolMaxMB` | 0 auto | 8192 | 4096 | 2048 | live |
| `BDMergeTexPoolFraction` | 0.50 | 0.25 | 0.25 | 0.125 | live |
| `BDMergeTexPoolFloorMB` | 1024 | 1024 | 512 | 512 | live |
| `BDMergeMeshPoolEnable` | 1 | 1 | 1 | 1 | live |
| `BDMergeMeshPoolMaxMB` | 0 auto | 2048 | 1024 | 512 | live |
| `BDMergeMeshPoolFraction` | 0.10 | 0.0625 | 0.0625 | 0.03125 | live |
| `BDMergeMeshPoolFloorMB` | 256 | 256 | 256 | 256 | live |
| `BDMergeDecodeThreadCeiling` | 24 | 12 | 8 | 4 | **restart** |
| `BDMergeCacheIOThreads` | 4 | 4 | 2 | 1 | **restart** |
| `BDMergeTexPoolRegionGraceTTL` (s) | 900 | 600 | 300 | 120 | live |
| `BDMergeTexPoolRecencyWindow` (s) | 300 | 300 | 180 | 120 | live |
| `BDMergeTexSpikeLog` | 1 | 0 | 0 | 0 | live |
| `BDMergeCaptureModePin` | 0 | 0 | 0 | 0 | live |

Verify each memory control name/default against `settings.xml` + `bdmergetexpool.cpp` / `bdmergemeshpool.cpp` before stamping; if a name differs, use the real one.

---

## 3. Why the memory defaults are wrong for 16–32 GB (rationale — keep)

- Texture pool defaults to **50%** of physical RAM (`settings.xml:~5853`, `bdmergetexpool.cpp:~138`); mesh pool **10%** (`bdmergemeshpool.cpp:~108`). Combined uncapped ceiling: 192 GB→115 GB, **32 GB→19.2 GB, 16 GB→9.6 GB** — the 16 GB case competes directly with the viewer heap, OS, shared iGPU memory, browser, and decode. Fixed caps fix this (Balanced ≈ 5 GB combined, Modest ≈ 2.5 GB).
- `MaxHeapSize64=0` → ~⅞ physical RAM but with a **16 GB floor** (`llappviewer.cpp:~1314`). On a 16 GB machine that floor expresses zero OS headroom → the low-memory texture-purge defense never fires → paging. Explicit 14 GB (Balanced) / 12 GB (Modest).
- VRAM budget: the target-texture formula (`llviewertexture.cpp:~577`) aims ~80% of the configured budget (≥512 MB reserved): 12288→~9830 MB, 8192→~6554 MB, 4096→~3277 MB — deliberately conservative for iGPU/shared-memory/desktop-composited setups.
- `CacheSize` (`settings.xml:~5941`, clamped `llappviewer.cpp:~4528`) is **disk**, not RAM — lowering protects SSD space/maintenance time only.
- Decode threads already `clamp(cores-6, 2, ceiling)` (`llappviewer.cpp:~2255`); the lower ceilings stop a 16 GB box decoding faster than memory/upload can absorb.

---

## 4. ReShade-absent behavior (confirmed)

The viewer is **fully functional without ReShade / `sl_reshade_bridge`** — no SDK or link-time dependency (`llreshadebridge.h:~7`), exported interface returns null on ABI mismatch (`llreshadebridge.cpp:~102`). BUT `gatherFrame()` is called **unconditionally every frame** (`llviewerdisplay.cpp:~1590`; small CPU descriptor work, no GPU). The expensive outputs are already gated (`RenderVisibleDiffuseSidecar=0`, `BDMergeVelocityBuffer=0`, `BDMergeMotionBlur=0`).

Panel presentation: an **"External ReShade integration"** master toggle (new setting §5.2) + note *"Optional. Alchemy-Machinima works normally without ReShade or the bridge add-on."* + advanced children ("Visible-diffuse/coverage output", "Motion-vector output") each with an explicit GPU/VRAM warning. **Never** gate on DLL detection — a missing/old/incompatible add-on must stay equivalent to "no consumer."

---

## 5. New code gates (exactly three — keep minimal, all default = today's behavior)

### 5.1 Wire the existing `RenderBloomHDR` (biggest fork GPU win, small surface)
`RenderBloomHDR` is defined (`settings_alchemy.xml:~1656`) but **not consumed**. Gate bloom-pyramid **allocation** (`pipeline.cpp:~1190`) and **dispatch** (`pipeline.cpp:~11037`) with `RenderHDREnabled && RenderBloomHDR`. Default stays `1` (identical output). When false: HDR exposure/tonemap stays active, but the bloom pyramid is neither allocated nor rendered. Confirm the exact cached-settings refresh path so a live toggle reallocates correctly.

### 5.2 Add `RenderReShadeBridgeEnabled` (Boolean, default `true`)
Gate the per-frame `gatherFrame()` + interface publication (`llviewerdisplay.cpp:~1590`, `llreshadebridge.cpp:~145`). When false: skip the gather and publish no bridge data — **without** overwriting the user's stored child-setting values (sidecar/velocity/motion). Default true → existing users unaffected; tier bundles set it 0 on High/Balanced/Modest.

### 5.3 Add `AlchemyCompatibilityTier` (U32, default `0`) — UI state only, NOT a render gate
`0 Flagship, 1 High, 2 Balanced, 3 Modest, 4 Custom`. Records the last-applied tier for the label. **Do not auto-apply on install/upgrade** — persist `0` as the identity but change no settings until the user clicks Apply. (The stock hardware recommender already caps auto-recommendations below Ultra at `llfeaturemanager.cpp:~616`; leave that path alone.)

No other new gates. Do NOT add SSS/area-light/water-quality gates — SSS/area-lights don't exist, and the water target (`mWaterDis`) is shared with atmospheric-depth duties (`pipeline.cpp:~1283`) so changing its allocation is a riskier render-graph change out of scope for slice 1 (use `RenderTransparentWater` for Modest).

---

## 6. UI (new Preferences tab + overlay apply)

### 6.1 Architecture anchors (verified)
- Floater `floater_preferences.xml`; tab host `tab_container "pref core"` (ends `:~198`); **Graphics** entry is `panel_preferences_graphics1.xml` at `:~111`. **Insert the new tab immediately after Graphics.**
- Panel classes register via `LLPanelInjector<T>` (`llpanel.h:~302`); e.g. graphics registered `llfloaterpreference.cpp:~3066`. A generic `class="panel_preference"` auto-saves/cancels any bound `control_name`, but the compat panel needs a **specialized** class (below).
- Quality preset bundle path: slider `Pref.QualityPerformance` → `LLFloaterPreference::onChangeQuality()` (`:~1974`) → `LLFeatureManager::setGraphicsLevel(level, skipFeatures=true)` (`llfeaturemanager.cpp:~696`) → `applyFeatures()` bulk-stamp (`:~641`). Reuse this for the base level.
- Reset reference: `Pref.HardwareDefaults` → `setHardwareDefaults` (`:~1498`) → `setRecommendedSettings` (`:~1509`).
- Apply/Cancel loops over panels: `:~1185–1198` / `:~1253–1259`.
- Generic snapshot/restore only covers **bound** controls (`:~2855` / `:~2952`).

### 6.2 `LLPanelPreferenceCompatibility` (new)
Create `panel_preferences_compatibility.xml` + `LLPanelPreferenceCompatibility : public LLPanelPreference`, registered with `LLPanelInjector`, tab added after Graphics. Layout:
- **Machine tier** radio/combo: Flagship / High / Balanced / Modest / Custom.
- **Apply tier** button + a compact summary of what will change; wire a new `Pref.*` callback in the floater ctor (`:~340–374`).
- **GPU & Visual** group, **Memory/VRAM & Cache** group, **ReShade Integration** group — grouped individual knobs (each a normal `control_name`, with per-slider reset buttons in the fork's existing `Refresh_Off` style).
- **Restart-required banner** shown when a restart-only setting (heap/cache/decode/cache-IO/10-bit) changed this session.
- "Restore current defaults" + normal Cancel.

### 6.3 `applyCompatibilityTier(tier)` order
1. Validate the tier enum.
2. `setGraphicsLevel(baseLevel, /*skipFeatures*/true)`.
3. Stamp that tier's exact overlay values (§2a/2b/2c) via normal `gSavedSettings` setters.
4. Let existing listeners rebuild shaders/RTs — **never allocate/destroy GL from the panel.**
5. Record `AlchemyCompatibilityTier`.
6. If any grouped control changes afterward → show tier = **Custom**.
7. Restart-only settings → record + show the restart banner; do not live-recreate GL context or worker pools.

### 6.4 Hard constraints (UI)
- **Specialized snapshot:** the panel MUST snapshot its own **fixed list** of every setting in the bundle and restore it on Cancel (the generic snapshot won't, since many bundle settings have no bound control).
- **Do NOT** route bundles through `LLPresetsManager` — besides poor memory/ReShade fit, it has a real string-literal-concatenation bug (`llpresetsmanager.cpp:~343` → `RenderReflectionProbeLevelRenderCASSharpness`). (Optionally flag that bug for a separate fix; not part of this slice.)
- **Do NOT** apply the Director-console-superset rule here — this is global app config, not machinima session state. If Director wants an entry point, add a button that opens this Preferences tab; don't duplicate controls.

---

## 7. Invariants
1. **Flagship default = today, byte-identical.** No behavior/perf change until the user applies a non-Flagship tier. Nothing auto-applied on install/upgrade.
2. **Reuse the stock bundle path** (`setGraphicsLevel` + `applyFeatures`) for the base level; only overlay the deltas. Don't reinvent feature application.
3. **Fail-soft + reversible:** invalid tier → no-op; Cancel fully restores the snapshot; restart-only changes clearly flagged, never a live GL/context/pool rebuild from the panel.
4. **Three new gates only** (§5), each defaulting to current behavior. Existing users who never open the tab are unaffected.
5. **Client-only**, no shaders/reserved samplers, no network, no new assets.
6. **No regression** to the stock quality slider, feature manager, first-run GPU recommender, or the preset manager.

## 8. Deliverables
1. **Research writeup** — confirm/expand §1–§6 against the live tree (verify every control name + anchor; correct any that differ), including the exact bloom control names and the `mWaterDis` sharing note.
2. **Git-applicable patch** (no build): the four tier bundles as data + `applyCompatibilityTier()`; the three gates (§5); `panel_preferences_compatibility.xml` + `LLPanelPreferenceCompatibility` + tab insertion + callbacks + specialized snapshot/restore + restart banner; settings.xml additions for the 3 new controls (defaults = current behavior). Cite file:line for every touched point; state new vs modified; confirm the Flagship-byte-identical claim.
3. A note on any control name that did not match this brief (so the reviewers can reconcile).

## Appendix — key anchors
- Preset engine: `llfeaturemanager.cpp:~186` (level names), `:~641` (applyFeatures), `:~696` (setGraphicsLevel), `:~616` (recommender); `featuretable.txt:~93–335`.
- Preferences: `floater_preferences.xml:~111` (Graphics)/`:~198` (tab host end); `panel_preferences_graphics1.xml`; `llfloaterpreference.cpp:~340–374` (callbacks), `:~1974` (onChangeQuality), `:~1498/1509` (defaults), `:~2855/2952` (snapshot/restore), `:~3066` (panel registration).
- HDR/bloom: `pipeline.cpp:~1081/1141/1190/11037`; `RenderHDREnabled` `settings.xml:~22576`; `RenderBloomHDR` `settings_alchemy.xml:~1656`.
- Memory: `settings.xml:~5853` (tex pool 50%), `bdmergetexpool.cpp:~138`, `bdmergemeshpool.cpp:~108`, `llappviewer.cpp:~1314` (heap floor)/`:~2255` (decode threads)/`:~4528` (cache clamp); `llviewertexture.cpp:~577` (VRAM target ~80%).
- ReShade: `llreshadebridge.{h:~7,cpp:~102/145}`, `llviewerdisplay.cpp:~1590`.
- Preset-manager bug (avoid / optional separate fix): `llpresetsmanager.cpp:~343`.
