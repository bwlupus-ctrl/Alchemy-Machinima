# Alpha-cutout shadow cascade cap (`RenderShadowAlphaNearCascades`): design brief, R3

**Status:** design only. Nothing is implemented, built or committed.

**Base:** HEAD `5e1134e5521` **plus Codex's uncommitted ProbeCost working tree**. Every file:line is
anchored to the current working tree (post-ProbeCost). Re-anchor if ProbeCost changes again.

**Evidence labels:**
- **[CODE]** = read in source at the cited line.
- **[INF]** = inference.
- **[WIP]** = ProbeCost code that is in the tree but uncommitted.

## 0. Review resolution table

| # | Source | Item | Resolution |
|---|---|---|---|
| R1-A1 | Codex R1 | The whole-block gate dropped rigged casters, avatars and alpha blend | Only the **static** pass (`i == 0`) of both alpha loops is gated. Rigged casters and the avatar pool are never capped. `POOL_TREE` is gated separately (§3.2). |
| R1-A1+ | Claude | Exempt non-rigged attachments if cheap | Draws can be told apart one by one (§3.3), so this is done through the existing rain predicate. Fallback: §3.4. |
| R1-A2 | Codex R1 | The zone count is not a valid verdict | Replaced by the counter in §6.3. |
| R1-A3 | Codex R1 | Subject-mode labels | The labels no longer carry numbers, and the coincidence is disclosed (§5). |
| R1-B1..B3 | Codex R1 | Edges, Reuse, baseline | Moved to §8. |
| R2-V1 | Codex verify | "Zero cost when off" is false: renderAlphaObjects gains a default-path branch | Reworded everywhere to **"identical output; one loop-invariant, predictable branch"**. The flag is hoisted above the loop, and the test sits at the top of the non-rigged branch before the per-draw binds at pipeline.cpp:11906/11915 (§3.3). |
| R2-A1 | Opus | A missing or misnamed key must never enable the cap: `get<T>` returns `T()` (that is, 0) for unknown keys (llcontrol.h:243-246) | **Re-encoded:** 0 = off (stock); 1-3 = the number of near cascades that keep alpha casters. Renamed to `RenderShadowAlphaNearCascades` so the old meaning cannot be misread. Combo values and presets updated (§3.1, §5). |
| R2-A2 | Opus | The filter leaked into the rigged pass (`pushMaskBatchesIndexed` tests it at lldrawpool.cpp:1261) and into the beauty pass | A conditional scoped guard wraps **only the `i == 0` body** of both loops. A debug-gated FAIL(leak) check runs at renderShadow entry and at the end of generateSunShadow (§3.3, §6.3). |
| R2-A3 | Opus | "Solid objects unchanged" is false: the render pass is chosen per face from its alpha mode | Every "solid unchanged" claim is removed. The UI is renamed "Alpha-cutout shadows (distant)", and the tooltip says that opaque-looking objects with alpha textures are affected. The §6.4 town row is fixed (§4.2, §5). |
| R2-A4 | Opus | The verdict counter double-counts across the scalar and indexed sweeps | Counting happens after the slot-count filters (the filter test moves below them). The grass and tree-pool skips are counted. The debug path logs an expected count, and cascades are identified by `sCurCameraID` (§6.3). |
| R2-B1..B4 | Opus | Camera-dependent cut-off; volumetrics brighten; Prism, mirrors and Reuse disagree with the direct view; standalone animesh prims are capped | Added to §8. A ghost-clone attachment check is added to §6.2. |
| R2-C2 | Opus | Test placement in renderAlphaObjects | Folded in (see R2-V1). |
| R2-C1 | Opus | "Cascade 0 always draws trees" is false (it can be skipped at ~27560) | Wording corrected in §3.5: cascade 0 can be skipped, which is harmless because polygon-offset fill is never enabled during sun shadows and every enabling site sets its own values. |
| R2-C3 | Opus | With Sharp range at 0, "Near" is the stock cascade-0 distance, which moves with the camera-to-sun angle | Stated in the tooltip (§5) and in §8.4. |
| R2-CN | Opus | Compile notes | Default arguments go in the declarations: `renderGeomShadow` at pipeline.h:623, `renderShadow` at pipeline.h:743-748. **Every** `sRainOcclusionRender` use is listed in §3.3. |

## 1. Motivation and an instrument caveat

From the user's Tracy capture:
- The frame takes 46.6 ms, and the GPU is about 40 ms of that.
- Shadow maps take about 18 ms over about 9.2 `renderShadow` passes.
- About 13.8 ms is attributed to "shadow alpha", "shadow alpha grass" and "shadow alpha material".
- "shadow simple" takes about 2.3 ms.

The three zones are **nested** [CODE]:
- "shadow alpha" (pipeline.cpp:26382) wraps 26380-26507.
- "grass" (26419) sits inside it.
- "material" (26430) sits inside "grass".

`LL_PROFILE_GPU_ZONE` is `TracyGpuZone` (llprofiler.h:165), and Tracy totals are inclusive. If 13.8 ms
is a sum of the three names, the real alpha cost is somewhere in [4.6, 13.8] ms.

## 2. Where shadow casters are drawn [CODE]

`renderShadow` is declared at pipeline.h:743-748 and defined at pipeline.cpp:26261. It has no
cascade index. Its body:

1. `updateCull` / `stateSort` (26307-26312).
2. **"shadow simple"** (26338-26367): every face whose pass is in `types[]` or `PASS_GLTF_PBR`.
3. `renderGeomShadow` (26377; body at 7910, pool test at 7929). The pools that cast:
   - terrain;
   - `LLDrawPoolTree`, which is alpha-cut (lldrawpooltree.cpp:173);
   - `LLDrawPoolAvatar` (lldrawpoolavatar.cpp:409-465).
4. **"shadow alpha"** (26380-26507), made of two loops over `i < pass_count`, where `i == 1` is the
   rigged pass:
   - **Loop 1** (26386-26450):
     - `PASS_ALPHA_MASK` (26397);
     - `renderAlphaObjects` (26404; body 11840, rigged split 11869, static binds 11906/11915);
     - `PASS_FULLBRIGHT_ALPHA_MASK` (26414);
     - `PASS_GRASS`, static (26423-26426);
     - legacy material masks (26431-26434), plus the indexed variants (26437-26447).
   - **Loop 2** (26452-26506): `PASS_GLTF_PBR_ALPHA_MASK`, both scalar and indexed.
   - Particles are absent from the render-type mask (26856-26923).

**Which pass a face takes** is chosen per face from its alpha mode, for example
llvovolume.cpp:7531-7586, where `ALPHA_MODE_MASK` goes to `PASS_GLTF_PBR_ALPHA_MASK` at 7584-7586.
Opaque-*looking* geometry therefore lands in an alpha pass if its texture or material is alpha-mask,
PBR MASK or alpha-blend. Alpha-blend is SL's default for any diffuse texture that carries an alpha
channel. [CODE+INF]

**renderShadow call sites.** There are five, all in pipeline.cpp:

| Line | What it draws |
|---|---|
| 26155 | Rain |
| 27508 | Subject column |
| **27908** | **Sun slice j** (loop at 27275, `sCurCameraID = CAMERA_SUN_SHADOW0+j` at 27282) |
| 28179 | Spot slots (main view only, 28159) |
| 28600 | Prism spot maps |

**Who runs generateSunShadow:**
- **Main view:** llviewerdisplay.cpp:843.
- **Cube faces** (reflection probes and hero mirrors): llviewerdisplay.cpp:1442, under `gCubeSnapshot`
  (llviewerwindow.cpp:6184), with 2 stretched cascades (27128-27133).
  - **[WIP]** Reuse mode skips generation (llviewerdisplay.cpp:1433-1446).
- **Prism** samples the main maps (llprismlens.cpp:7497-7511; pipeline.cpp:26751).

**Scope:** only call 27908, under `!gCubeSnapshot && mRT == &mMainRT`.

## 3. The change

### 3.1 Setting
- Name: **`RenderShadowAlphaNearCascades`**, S32, default **0**, Persist 1.
  - 0 = off (stock).
  - K = 1..3: only the K nearest sun bands draw static alpha-cutout casters.
  - Any value outside 1..3 means off.
- **A missing key reads as 0 (`get<T>` returns `T()`, llcontrol.h:243-246), which means off.**
- Add a static `S32 LLPipeline::RenderShadowAlphaNearCascades`, using the `RenderShadowAlphaMode`
  pattern: pipeline.cpp:344, 901 and 2059; pipeline.h:2207.
- Not part of `updateShadowEngaged` (pipeline.cpp:22575).
- No shader change.

### 3.2 Gate and slice mapping
- Add `include_alpha_cutout` as the last parameter of `renderShadow`. **The default `= true` goes in
  the declaration (pipeline.h:743-748).**
- Add `include_tree_pool` to `renderGeomShadow`. **The default `= true` goes in pipeline.h:623**, not
  in the definition at 7910.
- Change 26377 to `renderGeomShadow(shadow_cam, include_alpha_cutout)`.
- At 7929, add `&& (include_tree_pool || poolp->getType() != LLDrawPool::POOL_TREE)`.
- The avatar pool and the rigged pass (`i == 1`) are never gated.

```cpp
// generateSunShadow, slice call (27908); subject_active declared at 26962
bool alpha_casters = true;
const S32 k = RenderShadowAlphaNearCascades;
if (k >= 1 && k <= 3 && !gCubeSnapshot && mRT == &mMainRT)
{   // subject mode: slice 1 owns the near band (shadowUtil.glsl:826-835); slices 1-3 only
    const S32 max_j = subject_active ? llmin(k, 2) : k - 1;
    alpha_casters = (j <= max_j);
}
renderShadow(view[j], proj[j], shadow_cam, result[j], true, !have_union_cull,
             true, true, true, alpha_casters);
```

Static alpha-cutout casters are drawn in the following slices. Rigged casters, attachments, avatars
and the column are always drawn.

| UI item (K) | Normal layout | Follow subject |
|---|---|---|
| All distances (stock), 0 | 0-3 | column, 1-3 |
| Near only, 1 | 0 | column, 1 |
| Near + mid, 2 | 0-1 | column, 1-2 |
| All but far, 3 | 0-2 | column, 1-2 |

### 3.3 Attachment exemption through a scoped filter (recommended)

**Separability [CODE].**
- `mAttachedToAvatar` (llspatialpartition.h:135) is set per draw info (llvovolume.cpp:6135-6139).
- Attachment linksets get their own `LLAvatarBridge` (lldrawable.cpp:1303-1308, `PARTITION_AVATAR`
  at llvovolume.cpp:5388-5391), so a draw info is never part attachment and part world.

**Filter.**
- Replace the bool `sRainOcclusionRender` with `static U8 LLPipeline::sShadowDrawFilter`:
  - 0 = none;
  - 1 = reject attachments (rain);
  - 2 = attachments only (cap).
- **Every `sRainOcclusionRender` use changes:**
  - declaration, pipeline.h:1181;
  - definition, pipeline.cpp:587;
  - save / set / restore around the rain call, pipeline.cpp:26150-26158;
  - the predicate, lldrawpool.cpp:429-433, which becomes
    `f = sShadowDrawFilter; return f != 0 && p && (p->mAttachedToAvatar == (f == 1));`.
- The per-draw cost in the shared push loops is one byte load and one branch, exactly as today.

**Where the predicate runs [CODE].**

| Line | Function |
|---|---|
| 1074 | `renderGroup` |
| 1114 | `pushBatches` |
| 1137 | `pushUntexturedBatches` |
| 1202 | `pushMaskBatches` |
| 1261 | `pushMaskBatchesIndexed` |
| 2139 | `pushGLTFBatches` |
| 2158 | `pushUntexturedGLTFBatches` |
| 2181 | `pushGLTFBatchesScalar` |
| 2206 | `pushGLTFBatchesIndexed` |

- **Move the predicate below the slot-count test** in four of them:
  - 1202 below 1206;
  - 1261 below 1265, and still above the actor-FX test at 1269;
  - 2181 below 2185;
  - 2206 below 2210.
- Both tests only `continue`, so output is identical and branch cost is unchanged, and every draw is
  then filtered (and counted) in exactly one sweep.

**Scoping (R2-A2).**
- In **both** loops (26386, 26452), the `i == 0` body only:
  `std::optional<ShadowDrawFilterScope> f; if (!include_alpha_cutout && i == 0) f.emplace(2);`
  - The RAII scope saves the old value, sets 2, and restores at the end of the iteration, **before
    the rigged pass**.
- When capped, skip the grass call (26423-26426) and count it.
- **Default path:** one predictable branch per loop iteration.

**renderAlphaObjects (R2-V1, C2).**
- Hoist `const bool attach_only = (LLPipeline::sShadowDrawFilter == 2);` above the loop at 11864.
- At the **top of the non-rigged branch** (before 11904, ahead of the binds at 11906/11915), add
  `if (attach_only && !pparams->mAttachedToAvatar) continue;`.
- Claim: **identical output, plus one loop-invariant, predictable branch per alpha-blend shadow
  draw.** This is the only added default-path work.

**When capped:** the static loops still iterate and reject world draws on the CPU. No GPU draws are
issued for them. Shader binds run as stock.

### 3.4 Fallback (if §3.3 is rejected)
- Use `if (!include_alpha_cutout && i == 0) continue;` before the binds, in both loops.
- Non-rigged attachments are then capped (§8.6).

### 3.5 Default parity
- K = 0: every call passes `true` and the filter stays 0.
- Draw order, GL state, uniforms and targets are identical to stock. Trailing state is reset at
  26509-26513.
- `glPolygonOffset` values: the tree pool writes them (lldrawpooltree.cpp:167, 185), and a capped
  cascade skips that write.
  - Cascade 0 does **not** always draw trees: it can be cleared and skipped (pipeline.cpp:27560-27596).
  - This is harmless, because `GL_POLYGON_OFFSET_FILL` is never enabled during sun shadow passes, and
    every site that enables it sets its own values first (for example pipeline.cpp:8613-8614).
    [CODE+INF] (Opus C1)

## 4. Safety analysis

### 4.1 Transition [CODE]
- Slices span [dist[j]·(1-b), dist[j+1]·(1+b)] (27545-27546).
- The sampler cross-fades linearly across the band:
  - LEGACY path: shadowUtil.glsl:699-778;
  - UNITS path: shadowUtil.glsl:794-861.
- `b` is at least 0.08 (pipeline.cpp:22565).
- So the cap shows as a soft fade, not a cut. The edge caveats are in §8.1.
- Volumetric sampling (`nonpcfShadowAtPos`, shadowUtil.glsl:1173-) selects cascades with hard
  thresholds (§8.3).

### 4.2 Unaffected vs affected
**Unaffected:**
- every rigged caster;
- non-rigged worn attachments (§3.3);
- avatar body pool;
- subject column;
- projector shadows;
- faces whose pass is a "shadow simple" type;
- shadows that far casters throw onto near ground (`shadow_near_clip`, 26942-26947, applied at 27863).

**Affected beyond the cap:** every static face in an alpha-cutout pass, including **opaque-looking
buildings and rocks** whose texture or material is alpha-mask, PBR MASK or alpha-blend. Concretely:
- foliage;
- grass;
- fences;
- Linden trees;
- textured architecture that carries an alpha channel;
- non-rigged prims of standalone animesh (§8.5).

### 4.3 Caching and reuse
- Main maps are regenerated every frame (llviewerdisplay.cpp:843).
- Prism reuses the main maps in the same frame, so it inherits the cap.
- **[WIP] Reuse probes:**
  - they sample `mMainRT.shadow[]` (pipeline.cpp:22650, 26751);
  - selection is first-containing with no blend (shadowUtil.glsl:656-666);
  - the result is hard transitions that persist until the probe refreshes.
- Full probes and hero mirrors stay uncapped.
- Mode 2 has no probe sun shadow (shadowUtil.glsl:675).

### 4.4 Shadow Distance controls
- **Control A** (27085-27096) fixes clip0.
  - Without it, the splits come from the visible point cloud (`far_clip`, 27055) and the
    light-vs-camera split-exponent lerp (27068, with the 1.25 bump at 27077).
  - So the cut-off moves with camera angle and view distance (§8.4).
- **Control C** and ShadowMeta (27140-27164, 27885-) are not touched.
- **Control B (subject column)** is per §3.2.
- **Cull mode 1** (27181-27273) and `RenderShadowSplits` (27560-27596) behave as before.
- **`RenderShadowAlphaMode`** (pipeline.cpp:22828) is orthogonal.

### 4.5 Actor Mask, Tron, ReShade
- Actor Mask affects spot slots only.
- There is no Tron code in renderShadow or generateSunShadow.
- Sun maps are bound only by pipeline.cpp and shadowUtil.glsl. [CODE+INF]

## 5. UI, setting and presets

**settings.xml** (pattern: settings.xml:17886).
- `RenderShadowAlphaNearCascades`, S32, value 0, Persist 1.
- Comment: "Perf: 0 = stock. 1-3 = only the 1-3 nearest main-view sun cascades draw static
  alpha-cutout shadow casters (alpha-mask, PBR MASK and alpha-blend faces - foliage, grass, fences,
  and opaque-looking objects whose textures carry alpha). Rigged and worn items, avatars, projector
  shadows, the Follow-subject column, full reflection probes and mirrors are unaffected; Reuse
  probes and Prism inherit it."

**Lightbox › Shadows** (panel at floater_lightbox_settings.xml:2567).
- Add the row after the Alpha cutoff spinner (4038-4052), before "Shadow-casting projectors:"
  (4053).
- Use the `sd_alpha_mode_combo` geometry (3999-4028) and the reset pattern at 4076-4091.
- Raise the height at 2583 from 1100 to 1130.

Combo `sd_alpha_cap_combo`, label **"Alpha-cutout shadows (distant):"**. Items:
- "All distances (stock)" = 0
- "Near only" = 1
- "Near + mid" = 2
- "All but far" = 3

Tooltip:
> Faster shadows: surfaces drawn with alpha cut-outs or alpha blending cast sun shadows only near
> the camera. That covers leaves, grass and fences, and also some objects that LOOK solid, such as
> buildings or rocks whose textures carry an alpha channel. Avatars, worn items and projector
> shadows are unchanged.
>
> The cost: distant ground under trees and some alpha-textured buildings become sunlit. The shadow
> fades out where the shadow cascades meet. Volumetric shafts in forests get brighter. Mirrors and
> Reuse probes may disagree with the main view.
>
> With Sharp shadow range at 0, "Near" means the stock nearest-cascade distance, which moves with
> the angle between the camera and the sun and with how far you can see. Set Sharp shadow range for
> a stable distance. With Follow subject, "Near + mid" equals "All but far".

**Presets.** Add the setting to `getGraphicsControlNames` (llpresetsmanager.cpp:315-356). Its encoding
means presets that lack the key load as off.

## 6. Test plan (the user runs it)

### 6.1 Default parity (blocking)
- The baseline is the **post-ProbeCost build**.
- At K = 0, compare screenshots against that baseline and check that the Tracy zone set and counts
  are unchanged.
- Repeat with rain occlusion on, to prove the predicate fold and the slot-count reorder.
- Delete the key from user settings and confirm the cap stays off.

### 6.2 A/B
- Use Shadow Frusta debug and K = 1..3, with tree shadows placed across each split. Expect a soft fade.
- Repeat with:
  - Sharp range at 0 m (watch the cut-off move with camera pitch) and at 8 m (stable);
  - Follow subject;
  - minimum blend;
  - volumetrics on.
- Check an alpha-textured building at distance: it is expected to lose its shadow.
- Check avatars with prim hair and held props at 30-80 m: they must keep their shadows.
- **Ghost clones:** check whether their non-rigged attachments keep their shadows. This depends on
  `getAvatarAncestor()` for clone attachments; record PASS or FAIL.
- Check a standalone animesh with non-rigged prims: expected to be capped.
- Check Prism, a mirror and a Reuse probe.

### 6.3 Verdict and leak instrument
- Debug setting `RenderShadowAlphaCapDebug` (bool, default off, not persisted).
- Counters increment **only** in the filter == 2 branch, after the slot-count tests: `rej` for world
  draws, `att` for kept attachments. Also count:
  - the grass skip (adds `getRenderMapSize(PASS_GRASS)` to `rej`);
  - `renderAlphaObjects` non-rigged rejects;
  - the tree-pool skip (logged as `tree=skipped`).
- **Expected count (debug only)**, per capped cascade:
  - Sum `getRenderMapSize(type)` over the static alpha types that the static pass submits, minus the
    multi-material draws that no sweep runs for that type. Those are counted in the debug branch with
    the same slot tests, for example indexed sweeps that are skipped when their program is
    incomplete (26437, 26467).
  - Add the non-rigged `PASS_ALPHA` draws.
- **Cascade identity** comes from `LLViewerCamera::sCurCameraID` (27282), not from Tracy order.
- Every 64 frames, log `[AlphaCap] K=.. subj=.. cam=SUN_SHADOWj capped rej=.. att=.. exp=.. tree=..`.
- Verdicts:
  - **PASS:** `rej + att == exp`.
  - **FAIL:** a mismatch.
  - **INCONCLUSIVE:** `exp == 0`.
- **Leak check** (debug-gated):
  - At renderShadow entry, `sShadowDrawFilter == 2` logs `FAIL(leak)`.
  - At the end of generateSunShadow, a filter value other than 0 logs `FAIL(leak)`.
  - Rain legitimately holds 1 only inside its own call.
- **Time:** record per-cascade "shadow alpha" and `renderGeomShadow` times in Tracy, matched by
  camera ID and timestamp.
- **Expected saving [INF]:** (static "shadow alpha" share) + (tree share of `renderGeomShadow`).
  - Upper bound 1.5-3.4 ms per capped cascade.
  - "All but far" ≲ 1.5-3.4 ms; "Near only" ≲ 4.5-10 ms.
  - Lower if the 13.8 ms is double-counted (§1).

### 6.4 Scenes
| Scene | Expect |
|---|---|
| Forest | Maximum saving and change |
| Indoor or skybox | About 0 saving, no change |
| Town | Distant alpha-mask fences **and alpha-textured buildings** lose their sun shadow. Fully opaque-pass buildings are unchanged. |
| Crowd at 30-80 m | Unchanged |

## 7. Least-sure list

1. Is the predicate fold plus the slot-count reorder exactly rain-equivalent? Is any static alpha
   loop missing from the §3.3 table?
2. Does the scoped guard restore before the rigged pass on every path, including early `continue`s
   inside the `i == 0` body?
3. Is attachment separability true for indexed batches? `mAttachedToAvatar` comes from the first face.
4. Is the expected-count arithmetic (§6.3) exact for every sweep and program-availability
   combination?
5. Do ghost-clone attachments report `getAvatarAncestor()`?
6. Combo value 0 vs 1-3 round-trip, and the tree-pool iterator (7920-7960).
7. **Is it wise?** The saving shrinks if the 13.8 ms is double-counted, and the visual cost now
   includes alpha-textured architecture. Ship it opt-in; "All but far" plus a set Sharp range is the
   safe recommendation.

## 8. Known limits (documented, not fixed)

1. **Edge coverage is not proven seam-free.** Maps fit the visible point cloud (27560) with the near
   plane replaced (27863), and the inherited filter-edge and uv-guard limits apply. A pre-existing
   edge artefact can now coincide with the fade.
2. **Reuse probes** have hard, persisting transitions. **Prism** inherits the cap. **Full probes and
   hero mirrors** stay uncapped, so they can **disagree with the direct view** (Opus B3).
3. **Volumetrics:** hard cascade step at (1-b)·clip (shadowUtil.glsl:1173-). With fewer occluders,
   shafts in forests get **brighter** (Opus B2).
4. **The cut-off moves** with the camera-to-sun angle and the view distance unless Sharp range is
   set. At 0, "Near" is the stock cascade-0 distance (27055, 27068, 27077, 27085; Opus B1/C3).
5. **Standalone animesh non-rigged prims** are capped: `getAvatarAncestor()` excludes them
   (llvovolume.cpp:6132-6133) (Opus B4).
6. With fallback §3.4 only, non-rigged worn attachments are capped.
7. The CPU still sorts and iterates the alpha maps; the saving is GPU time.
8. Default parity holds against the post-ProbeCost baseline only.
