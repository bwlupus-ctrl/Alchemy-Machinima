# Adversarial review R1 (Opus): PROBE_UPDATE_ON_DEMAND_BRIEF_V2

Read-only review at HEAD c31f37cc43a. Every anchor cited below was read at HEAD.
**Verdict: GO-after-fixes.** The architecture holds up: per-probe serials, an ack only on a real radiance face 6, the structural limit of 1 ordinary face per frame, the Live and sliced paths never running in the same frame, D, C0 and deferring C1. The dirt-source layer does not hold up. It picks up camera-driven churn as "content change", which cancels the feature whenever the camera moves, and it misses several discrete sources that ordinary probes do render.

## P0

**P0-1 Terrain LOD churn dirties every probe whenever the camera moves.** `LLSurfacePatch::updateVisibility` (llsurfacepatch.cpp:1054-1070) calls `dirtyGeom()` on the patch and its W/S neighbours whenever the main-camera render stride changes. `LLVOSurfacePatch::dirtyGeom` (llvosurfacepatch.cpp:781-790) then calls `markRebuild(REBUILD_ALL)` plus `movePartition()`. H3 accepts this (the flag is not position-only). `classify` returns `C_TERRAIN_WATER`, which §3.3 treats as **always hits**, so every probe is dirtied, including the default probe and skybox probes thousands of metres above the terrain. The same pattern shows up in tree trunk LOD (llvotree.cpp:376-380), grass blade count (llvograss.cpp:~353-378) and texture-anim matrix toggles driven by virtual size (llvovolume.cpp:~665). Expected results: T6 and T7 fail, and the feature does nothing on any sim with terrain while the camera moves. Separately, with the recorder on, **Live On change never idles during camera moves**: `mLiveSceneSerial` goes up every frame, so turning on RenderProbeOnDemand regresses 615750a40f9.
*Fix:* wrap the camera-driven rebuild sites (stride block, tree/grass LOD, tex-anim matrix toggle) in `ScopedTag(TAG_LOD)`. Keep real heightfield edits as events (`dirty()` :98, `updateVerticalStats`→`dirtyPatch` :611, composition :882). Add llsurfacepatch/llvotree/llvograss to the allowed files. Add per-class drop counters to the log.

## P1

**P1-1 The LOD tag misses its own consequences.** For a LOD increase, `lodOrSculptChanged` sets `should_update_octree_bounds` (llvovolume.cpp:2124), so `genBBoxes`→`movePartition` (:1967) changes the bounds. H4 ("content || bounds changed") and H1b fire **outside** the tag scope at :1719. *Fix:* when a rebuild is LOD-only (`mLODChanged` and no other content flag, not REBUILD_POSITION), run the whole `updateGeometry` under TAG_LOD.

**P1-2 Ordinary probes light from the main-eye `mNearbyLights`.** pipeline.cpp:9737 says so explicitly ("Normal probes deliberately reuse the main-eye list"), and the loop at :23263+ confirms it. What a probe captures depends on camera distance (RenderFarClip), the RenderLocalLightCount cap, `sRenderAttachedLights`, the BDMerge own/other/world toggles and muted/too-complex avatars. None of these produce an event. **Attachment child-prim lights** (facelights, held-prop lights) move with the avatar's bridge. Only the attachment root and its bridge get `updateMove` (llvoavatar.cpp:3176-3200), and `classify` drops the root as DYN. So the claim that "attachment lights are NOT dropped" is **false in practice**. *Fix:* replace or extend H5 with a flush-time diff of `mNearbyLights`: membership plus quantised position, radius, colour and spot params for each light (≤256 entries, trivial cost), emitting C_LIGHT events. Add the light toggles to the ordinary env tail.

**P1-3 `recording()` "reads a file-static bool".** If implemented literally, an inline function in the header reading `static bool` gives each translation unit its own copy. Hooks in other files would never see `setRecording`, so the feature silently does nothing. *Fix:* use one `extern bool` (or a C++17 `inline` variable) defined once.

**P1-4 Texture streaming churn.** H6 fires on every discard-level upload that main-camera priority drives. A texture shared by more than 4096 faces triggers `markOverflow` and dirties every probe. *Fix:* publish only the first real image, plus refinements that cross a coarse level relevant to probes (e.g. discard ≤ log2(tex/probeRes)). Also fan out `mVolumeList[LIGHT_TEX]` so that a projector gobo arriving calls `noteLight` (currently missed).

**P1-5 Flush cost.** The cap of E×P ≤ 200 000 with two capsule tests each could mean several ms on the main thread during load storms, which is the thing this feature is meant to save. *Fix:* cap near 16k. Test the sphere first and only run the capsule tests if it fails, or broad-phase against a union bound per probe.

## P2

- R_PROBE should also compare probe ambiance, near clip, `getIsDynamic()` and box/sphere (PARAMS_REFLECTION_PROBE edits are not hooked). Also resync on RenderReflectionProbeDetail changes. Test LIGHT **before** the `isReflectionProbe()` drop.
- Missed discrete sources: `setGoboOverride`/no-shadow set (not parameterChanged), and Rig Rim settings when `CineRigRimIncludeProbes`=1. Add these to the ordinary env tail.
- `shift()`: offset the queued events instead of `markOverflow`. Every region crossing currently costs a full 12P-face resync.
- `onTxnComplete` should do nothing when `!mInTxn`. A txn that started while OFF and completes while ON currently stamps `mLastComplete`. Resync on the ON edge should also clear stale `mInTxn`.
- Specify that H4 notes union(entry, exit). H1b's diff is almost always a no-op for volumes (the extents are already new at `move()` entry). That's harmless, but say it.
- D: PASS_GLTF_PBR groups can be emissive. Skip a group only if none of its GLTF materials has a non-zero emissive factor or texture.
- OVER BUDGET is structurally unreachable for ordinary and Live faces. It is an implementation-bug detector only, so say so. STARVED will fire routinely during the day cycle when P ≥ ~25.
- Test plan: Rcap is about 131 m, so "distant probes stay clean" (T2) holds only for probes more than 131 m away. T6 and T7 predictions are wrong until P0-1 and P1-1 are fixed.
- Anchors: settings.xml is **:20703-20713**, not :703. The Lightbox insertion must go right after :1537, **before** the `<!--` at :1538-1576. Inserting between :1537 and :1577 can land inside a comment, and the UI would silently vanish.
- Recommend a "Refresh all probes now" button (manual R_RESYNC) for use before a take.

## Q&A

- **MaxAge 60 s as a backstop:** acceptable for the continuous sources in §3.8. It is not acceptable for the discrete misses above (P1-2, P2): a stale reflection that lasts 60 s is a continuity error across takes.
- **Budget:** 1 ordinary face per `update()` (continuation at :354 or a start at :619, gated by `did_update`). Realtime is Live FULL 6, Live budget ≤ 6 or sliced N, and these are mutually exclusive through `realtime_probe`. The default probe uses the ordinary path. So the total is ≤ 7, the same as today. **Holds.**
- **D3 (avatars):** right for geometry. Attachments use LLAvatarBridge with RENDER_TYPE_AVATAR, and static probes exclude it (llviewerwindow.cpp:6090-6108). Wrong for attachment lights (P1-2).
- **OFF path:** holds once P1-3 is fixed and each hook's snapshot sits inside `if (recording())`. Live H is unchanged because `addExact` mixes into `mExact` and the StickyHash layout is untouched.
- **C0/C1/D isolation:** sound. `sCaptureKind` is scoped by RAII, Hero never enters `updateProbeFace`, and shadow passes are excluded.
