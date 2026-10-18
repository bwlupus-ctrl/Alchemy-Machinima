# Projector flare flicker diagnosis (Codex, read-only, 2026-10-18)

**Committed-HEAD diagnosis — `7050a38cc4e..88c254041fc`**

HEAD’s renderer matches reported build `ffbd7be775b`; subsequent commits affect tests/documentation. Uncommitted edits were excluded. **No definite corruption bug was established by static inspection.**

The key dependency is:

`projector cookie + spot-shadow depth + scene depth → isolated shaft texture → flare threshold`

The flare takes the **maximum of nine shaft-luma samples**, then applies a threshold (`postEffectUtilsF.glsl:790`, `:809`). CPU visibility smoothing does **not** smooth this sampled energy. Small upstream fluctuations can therefore blink the entire optical stack.

**Ranked suspects**

1. **Direct shadow FBO transitions — first regression A/B; unconfirmed.**  
   Changed entry: `indra/newview/pipeline.cpp:30077`; projector writes: `pipeline.cpp:31198`, `:31213`; transition implementation: `indra/llrender/llrendertarget.cpp:737`, `:771`. Each completed shadow target now remains bound until its sibling replaces it; parent/default-FBO restoration occurs once at sequence completion. A wrong-target depth write or attachment/driver regression would alter `mSpotShadow[i]`, change marched shaft visibility, and cross the flare threshold.  
   **Evidence limiting confidence:** pending batches drain before transitions and after passes; each bind selects its own buffers/viewport; final restoration exists. No intervening erroneous depth draw or read-before-write was identified. This is not evidence of a missing barrier.  
   **Live switch:** `RenderShadowDirectTransitions=false`.

2. **Media cookie content and update cadence — applicable only to media-fed projectors.**  
   New preparation/cadence: `pipeline.cpp:2372`, `:2377`; publication: `:2538`; cookie selection: `:28064`; shaft consumer: `shaders/class3/deferred/projectorVolumetricF.glsl:595`. Video brightness, black letterboxing, grain/dropout/flicker, and cadence-limited effect updates now change the cookie sampled throughout the shaft. Those changes can move the nine-sample maximum across the flare knee, producing flare-wide brightness jumps without damaged texels.  
   **Evidence limiting confidence:** copy precedes preparation; cookie mips finish before publication; source identity and accepted settings accompany retained pixels. The camera feed retains its existing preparation path.  
   **Live switch:** `VCamMediaSlotsEnabled=false`. This removes media routing, so disappearance alone does not prove corruption. **No dedicated switch bypasses only the new cookie pass.**

3. **New volumetric ownership release gate — lifecycle-dependent.**  
   Changed gate: `indra/newview/llprismlens.cpp:1978`, `:6474`; ownership release: `pipeline.cpp:22537`. Disabling Prism/media, stopping a private source, or replacing an emitter can release automatically owned shaft/master enables. Losing the last owner removes shaft energy; the flare may switch from sampled live energy to its modeled release tail. Repeated lifecycle transitions could appear as flicker.  
   **Evidence limiting confidence:** unchanged target identity returns early (`llprismlens.cpp:1987`); no spontaneous per-frame oscillation was found. For a stable camera source with Prism enabled, eligibility is unchanged.  
   **Live switches:** `BDMergeProjectorVolumetrics=false` removes the producer; `PrismLensEnabled=false` exercises release but also removes VCam rendering. **No dedicated ownership-gate bypass exists.**

4. **Changed rigged geometry alters occlusion — low confidence; expect silhouette-linked flicker.**  
   Changed LOD decision: `indra/newview/llvovolume.cpp:1720`. Retaining a higher rigged LOD changes geometry written into scene depth and projector shadows. That can change both shaft-luma probes and the flare’s per-pixel glow/star occlusion (`postEffectUtilsF.glsl:863`). This predicts changes around moving avatar/attachment edges, rather than arbitrary corrupted blocks.  
   **Live switch:** `RenderRiggedLODHysteresis=0`.

**Flare input inventory and frame production**

Paths below are under `indra/newview/`; shader paths under `app_settings/shaders/`.

| Read/input | Producer, binding, and range assessment |
|---|---|
| `uCineFlareShaft` | `mProjVolHalf`, or freshly resolved `mProjVolHistory[cur]`; produced at `pipeline.cpp:21760`, `:22276`, published at `:22356`, bound bilinear at `:14155`. Validity resets each main pass (`:21658`); full clear and history ping-pong are unchanged. |
| `depthMap` | Main deferred depth from scene rendering; bound at `pipeline.cpp:13645`. Used for projector glow/star occlusion. Geometry changes can alter contents; binding is unchanged. |
| `diffuseRect` | Current HDR scene, normally `mRT->screen`; point-bound at `pipeline.cpp:13644`. **Live projector flare energy uses the isolated shaft sampler**, while sun flare samples this scene texture. |
| Cookie and spot-shadow depth, indirectly | Cookie bound at `pipeline.cpp:28127`; shadow target selected at `:29685`; geometry/cookie uniforms uploaded by `:28420`. Changed cookie media path and shadow transitions are the principal producer changes. |
| CPU source state | Lit rig slots, emitter identity/on/intensity, volume transform/scale/base color, camera matrices, frame delta and release caches; reconstructed/uploaded at `pipeline.cpp:13963`, `:13977`, `:14026`, `:14175`. Flare code is unchanged. Only cinematic-rig projectors are enumerated; an arbitrary standalone projector is not itself a cine-flare source. |
| Final composition | Writes `postPingMap` through `colorCorrect` (`pipeline.cpp:23818`). Depth test/write are disabled (`:13614`). Bloom, exposure and grading subsequently affect appearance; they do not supply the live projector’s threshold energy. |

Uniform inventory: `uResolution`; `uCineFlare{Count,A,Color,Threshold,ProbeRadius,Shaft}`; and `uLensFlare{Strength,SunPos,SunVisibility,LightColor,OcclusionRadius,OcclusionTaps,StreakLength,StreakFalloff,StreakWidth,StreakIntensity,StreakTint,ChromaticSpread,Glow,GlowRadius,GlowFalloff,Ghost,GhostCount,GhostSpacing,Halo,HaloRadius,HaloWidth,Starburst,StarburstSpikes,StarburstSharpness,StarburstFalloff,GhostChroma,HaloChroma,Iris,IrisCount,IrisSides,IrisSize,Ring,RingRadius,RingWidth,RingDispersion,RingCount,Circle,CircleScale,CircleSpacing,CircleCount,Arc,Warp,StreakTipAmount,StreakTipTint,SrcColorAmount,Master}`. These come from settings/source calculations in `pipeline.cpp:13673`, `:13698`, `:13834`, `:13903`. `sky_hdr_scale` affects the sun branch, not projector energy. None changed here.

`tonemapUtilF.glsl` contains exposure/tonemapping, not projector-flare generation. It, `colorCorrectF.glsl`, `postEffectUtilsF.glsl`, and the volumetric march shader have identical Git blobs at both endpoints.

**Checks that lowered other suspects**

- New media passes restore FBO, viewport, scissor, depth, blend and color-mask state through `ScopedPrismRenderState` (`llprismlens.cpp:9497`, `:10886`); sRGB is locally scoped. Borrowed media texture/sampler bindings are restored (`:3171`, `:3198`).
- Cookie `TMG_AUTO` generates mips before publication (`pipeline.cpp:2538`; `llrendertarget.cpp:688`). Texture disabling drains queued immediate draws (`indra/llrender/llrender.cpp:428`). No missing flush, uninitialized-cookie publication, or required memory-barrier omission was identified.
- VBO cleanup changes expiry timing (`llvertexbuffer.cpp:548`; `llviewerdisplay.cpp:1238`), but still expires pooled buffers older than five seconds. No live-buffer deletion was established. Fallback A/B: `RenderVBOPoolCleanAtFrameEnd=false`.
- Probe signatures now include media cookie revisions (`llreflectionmapmanager.cpp:1645`; `pipeline.cpp:10922`). This can increase probe work/change scene reflections, but probes do not supply isolated shaft energy. No dedicated signature-change kill switch exists.

**A/B order**

Restore each setting before the next test; retain identical camera, lighting and playback.

1. `RenderCineLensFlareIntensity=0`: isolate the visible flare contribution while preserving projector-source mode and sun suppression.
2. Restore intensity; set `RenderShadowDirectTransitions=false`.
3. For media sources, test `VCamMediaSlotsEnabled=false`, then compare a frozen/static source with animated projector effects disabled.
4. Test `BDMergeProjectorVolumetricsTemporal=false`; separately test `BDMergeProjectorVolumetricsTemporalDither=false`. These localize the unchanged shaft resolve/sampling path.
5. Test `RenderRiggedLODHysteresis=0`, then `RenderVBOPoolCleanAtFrameEnd=false`.
6. Finally, `BDMergeProjectorVolumetrics=false` confirms whether remaining corruption depends on the shaft producer.

