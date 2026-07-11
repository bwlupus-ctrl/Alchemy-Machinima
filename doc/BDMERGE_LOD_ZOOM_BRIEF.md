# Machinima High-LOD / Anti-Cull Mode — Investigation Brief

Scope: how the renderer picks mesh/prim LOD and culls distant/small objects, why
wide/zoomed-out shots lose detail and pop, which knobs already exist and where they
hard-cap, and a concrete gated "machinima" mode to push LOD quality over culling.
Target HW: RTX 5090 32GB / 192GB / 9950X — massive headroom, machine-specific tuning is fine.

All anchors are `file:line` in `I:\alchemy-machinima` unless noted.

> **STATUS: IMPLEMENTED (Batch 4, gated `BDMergeMachinimaHighLOD`, default OFF).** All four levers from §5/§8 landed — see the "Batch 4" row in `doc/BD_MERGE_PATCHLOG.md`. Deltas from the brief's original plan: the impostor kill (Lever B) and the FORCE_* relax (Lever C) are implemented by **gating the reads** (`llvoavatar.cpp` `isImpostor`/`shouldImpostor`/`isTooComplex`; `llvovolume.cpp` `machinimaForce*` accessors) rather than writing/save-restoring settings, so the user's saved impostor values are never clobbered and OFF returns byte-identical behavior. `MAX_LOD_FACTOR` is left at 4.0 for stock; a new `MACHINIMA_MAX_LOD_FACTOR = 32.0` is used only while the mode is on (both clamp sites), and `sDistanceFactor` is clamped to `llmax(0.1, 1 - lod*0.1)` (§6 landmine fixed). Lever E (occlusion relax) intentionally NOT implemented (guardrail). In-world acceptance owed. The rest of this brief is preserved as the original read-only investigation.

---

## 1. How LOD selection actually works

### 1.1 Entry points
- `LLVOVolume::updateLOD()` — `indra/newview/llvovolume.cpp:1618`; calls `calcLOD()` and, if the level changed, `markRebuild(... REBUILD_VOLUME)` (`:1640`). This rebuild-on-change is exactly what produces visible *popping*.
- `LLVOVolume::calcLOD()` — `indra/newview/llvovolume.cpp:1468` — the core selector.

### 1.2 The math (calcLOD)
```
distance = mDrawable->mDistanceWRTCamera            // :1520 (rigged: avatar distance :1491)
radius   = volume->mLODScaleBias.scaledVec(scale).length()   // :1521
lod_factor = LLVOVolume::sLODFactor                 // :1479  (= RenderVolumeLODFactor, clamped)
distance *= sDistanceFactor                          // :1553  (sDistanceFactor = 1 - sLODFactor*0.1)
// near-boost ramp when very close:                     :1555-1563
distance *= F_PI/3.f                                 // :1566
if(!IgnoreFOVZoomForLODs)                            // :1568-1572
    lod_factor *= DEFAULT_FIELD_OF_VIEW / camera.getDefaultFOV();
cur_detail = computeLODDetail(distance, radius, lod_factor)   // :1588
```
- `computeLODDetail()` — `:1412`: with `sDynamicLOD` (default on), `tan_angle = (lod_factor*radius)/distance`, then `LLVolumeLODGroup::getDetailFromTan()`.
- `getDetailFromTan()` — `indra/llmath/llvolumemgr.cpp:328`: steps through **fixed thresholds** `mDetailThresholds = {0.03, 0.06, 0.24, 3.0}` (`llvolumemgr.cpp:33-39`, `BASE_THRESHOLD=0.03`). Returns LOD 0..3. These thresholds are hard-coded constants, not settings.
- HUD attachments and (optionally) selected objects bypass the math and force `cur_detail = 3` — `:1577-1585`. This is the proof-of-concept for a forced-max-LOD path.

### 1.3 Is zoom / FOV factored in?  Partly — and only the *telephoto* case.
- `IgnoreFOVZoomForLODs` (default **0/false**, `settings.xml`) means the FOV term IS active. When you zoom in with Ctrl+0 the camera's `getDefaultFOV()` shrinks, so `DEFAULT_FIELD_OF_VIEW/getDefaultFOV()` > 1 and `lod_factor` rises → higher detail. Good.
- **This does nothing for the machinima wide-shot problem.** A wide establishing shot is *camera placed far from the subject* (large `distance`), usually at normal or wide FOV. Distance is the dominant term and it only ever *lowers* detail. There is no "the subject is small in frame, boost it" path except the manual telephoto zoom. So the classic failure — pull the flycam back 150m for a vista and everything degrades to LOD 0 — is by design and unmitigated.

### 1.4 Worked example (why distant detail dies even at max settings)
2m-radius mesh, `RenderVolumeLODFactor=4.0` (the hard max), no FOV zoom:
- at 200m: `tan = 4*2/200 = 0.04` → above 0.03 → **LOD 1**
- at 300m: `tan = 4*2/300 = 0.027` → below 0.03 → **LOD 0** (lowest)
Even at the ceiling, mid-size objects hit lowest LOD by a few hundred meters. Smaller props fall off much sooner. The only way to hold LOD 3 at vista distances is to **raise/uncap `MAX_LOD_FACTOR`** or **force the detail level directly**.

---

## 2. The caps (what limits you today)

| Knob | Default | Hard cap | Cap site |
|---|---|---|---|
| `RenderVolumeLODFactor` → `sLODFactor` | 2.0 | **4.0** (`MAX_LOD_FACTOR`) | `indra/newview/llvovolume.h:57`; clamp at `llviewercontrol.cpp:399` and `llappviewer.cpp:570` |
| `RenderAvatarLODFactor` | — | **1.0** (`MAX_AVATAR_LOD_FACTOR`) | `indra/newview/llvoavatar.h:80`; clamp `llviewercontrol.cpp:406`, `llappviewer.cpp:574` |
| `RenderFarClip` (draw distance) | 256 | UI 1024 (`floater_lightbox_settings.xml:1278`); code accepts more | `settings.xml` |
| `RenderAvatarMaxNonImpostors` | 12 | 0 = unlimited; slider max `NON_IMPOSTORS_MAX_SLIDER=66` | `llvoavatar.cpp:11376` |
| `RenderMaxNodeSize` (KB/octree batch) | 65536 | S32 | `settings.xml:11927` |
| `IgnoreFOVZoomForLODs` | 0 | bool | `settings.xml:5546` |
| LOD detail thresholds | {0.03,0.06,0.24,3.0} | **hard-coded** | `llvolumemgr.cpp:36-39` |

Existing UI already exposing these (our fork's Lightbox → LOD tab, `floater_lightbox_settings.xml`): Draw Distance (`:1283`, max 1024), Object LOD = RenderVolumeLODFactor (`:1318`, **max 4.0**), Max Non-Impostors (`:1388`), plus Tree/Avatar/Terrain LOD (`:1493-1610`). Advanced-prefs Objects slider also caps at 4.0 (`floater_preferences_graphics_advanced.xml:597`). So the LOD slider is already pinned to the code ceiling — you cannot push objects past 4.0 without a code change.

---

## 3. Culling paths

### 3.1 Frustum + far-clip (distance) cull — the main object remover
- `LLOctreeCull` — `indra/newview/llspatialpartition.cpp:1035`.
  - `frustumCheck()` / `frustumCheckObjects()` (`:1062-1080`) = `AABBInFrustumNoFarClip*` intersected with `AABBSphereIntersect*Extents`. The **sphere = camera far-clip sphere**, radius = `RenderFarClip`. So distance culling of whole objects is governed by `RenderFarClip`, nothing more granular.
- Practical effect: objects beyond draw distance vanish entirely (hard pop at the far plane). Raising `RenderFarClip` is the lever; there is no separate small-object distance cull that removes objects earlier.

### 3.2 Occlusion / HZB cull
- `sUseOcclusion` set from `RenderOcclusion` at `pipeline.cpp:1248` (0=off, 1, 2=full HZB). `earlyFail()` occlusion-culls whole octree groups (`llspatialpartition.cpp:1048-1057`). Never culls the root node.
- For machinima this occasionally *flickers* thin/edge geometry behind big occluders during camera moves. Optional to disable, but see caveats §6.

### 3.3 Small-on-screen face degradation (not a full cull, but detail loss)
- `FORCE_SIMPLE_RENDER_AREA = 512.f`, `FORCE_CULL_AREA = 8.f` — `llvovolume.cpp:97-98`.
  - Faces with `getPixelArea() < 512` are pushed to a "simple" (no full material) render path (`:6130`, `:7031`); geometry below `FORCE_CULL_AREA` is dropped from batching (`:7426`). These are **hard-coded constants**, contribute to the "material/detail flattens when small" look, and scale with screen resolution — at 4K/8K machinima renders more faces fall under 512px than the constant assumes.
- `MIN_TEX_ANIM_SIZE` gates texture-anim updates by virtual size (`:626` etc.) — minor.

### 3.4 There is no min-pixel-area *object* cull knob
Confirmed: no `sMinRenderSize` / `RenderMinimumDrawDistance` in the tree. Small distant objects are lost via (a) LOD → 0 (§1) and (b) far-clip (§3.1), not a dedicated small-object cull. So the fix surface is LOD + far-clip + the FORCE_* constants, not a cull-threshold toggle.

---

## 4. Avatars / attachments as impostors

- Impostor state is **rank/count based, not distance based**: `sMaxNonImpostors = 12` (`llvoavatar.cpp:613`), `sLimitNonImpostors` (`:614`). `isImpostor()` = visually-muted OR (`sLimitNonImpostors && mUpdatePeriod>1`) (`:11318`). `isTooComplex`/rank check `mVisibilityRank > sMaxNonImpostors*rank_factor` (`:11333`). So the *nearest N* avatars render full; the rest become 2D billboards regardless of how good your GPU is.
- `updateImpostorRendering()` (`:11380`): setting `RenderAvatarMaxNonImpostors = 0` (>= `NON_IMPOSTORS_MAX_SLIDER=66`) → `sMaxNonImpostors=0`, `sLimitNonImpostors=false` → **impostoring fully off**.
- Complexity/attachment impostor: `RenderAutoMuteSurfaceAreaLimit` (default 1000, `settings.xml:12769`) and `RenderAvatarMaxComplexity` — set to 0 to disable the auto-mute-to-impostor path.
- Wide shots with a cast of avatars: the ones farthest from camera flatten to impostors. For machinima you almost always want `RenderAvatarMaxNonImpostors=0` while filming.

---

## 5. Proposed gated "Machinima / High-LOD" mode

All default OFF. Gate everything behind one master debug setting, e.g. `MachinimaHighLOD` (bool), with the sub-levers also individually settable. Ranked by impact-for-machinima.

### Lever A — Uncap + force object LOD  (HIGHEST impact, LOW effort)
Two complementary pieces:
1. **Raise the ceiling.** `MAX_LOD_FACTOR` 4.0 → e.g. 16–32 (or make it a setting `RenderVolumeLODFactorMax`). Sites: `llvovolume.h:57`, and the two clamps `llviewercontrol.cpp:399` + `llappviewer.cpp:570`. Bump the UI `max_val` at `floater_lightbox_settings.xml:1313/1327` and `floater_preferences_graphics_advanced.xml:597`. Lets the existing slider push detail much farther before rolloff.
2. **Hard-force max detail when the mode is on.** In `calcLOD()` add, alongside the existing HUD branch at `llvovolume.cpp:1577`:
   ```
   else if (sMachinimaForceMaxLOD) cur_detail = 3;
   ```
   This reuses the proven HUD/selection force path (`:1580-1584`) and makes *every* volume render at LOD 3 regardless of distance — the cleanest kill for zoom-out LOD loss and the popping (since `cur_detail` stops changing, `updateLOD()`'s rebuild at `:1640` stops firing).
   Effort: ~10 lines + 1 setting + UI checkbox. This single toggle solves the primary complaint.

### Lever B — Disable avatar impostors while filming  (HIGH impact, TRIVIAL)
No code needed — drive existing settings from the mode: `RenderAvatarMaxNonImpostors=0`, `RenderAutoMuteSurfaceAreaLimit=0`, `RenderAvatarMaxComplexity=0`. Optionally raise `RenderAvatarLODFactor` cap (`MAX_AVATAR_LOD_FACTOR` `llvoavatar.h:80`, 1.0 → 2–4) the same way as Lever A so avatar mesh LOD can also be forced up. Effort: settings wiring + optional cap bump.

### Lever C — Raise draw distance & scale the FORCE_* constants  (MEDIUM impact, LOW effort)
- Push `RenderFarClip` (UI already to 1024; allow higher for vistas). Prevents far-plane object pop (§3.1). Pure setting.
- Make `FORCE_SIMPLE_RENDER_AREA` / `FORCE_CULL_AREA` (`llvovolume.cpp:97-98`) either resolution-scaled or overridable to ~0 in machinima mode, so small-in-frame faces keep full materials and aren't dropped. Effort: ~15 lines (turn the two consts into cached-control reads gated by the mode).

### Lever D — Factor camera distance/frame-coverage into LOD, or expose thresholds  (MEDIUM impact, MEDIUM effort)
If a global force-to-LOD3 (Lever A.2) is too heavy on huge scenes, instead expose `mDetailThresholds` (`llvolumemgr.cpp:36`) as a settings-driven array, or multiply `lod_factor` by a `MachinimaLODBias` in `calcLOD()` (`:1479`). Softer, tunable per-shot alternative to the hard force. Effort: threshold plumbing.

### Lever E — Optionally relax occlusion during a shot  (LOW impact, LOW effort, risky)
Let the mode set `RenderOcclusion` lower (0/1) to stop edge/thin-geo occlusion flicker during camera moves (`pipeline.cpp:1248`). Optional and off by default — see caveats.

**Recommended default bundle for the master toggle:** Lever A.2 (force LOD 3) + Lever B (no impostors) + Lever C draw-distance/FORCE_* relax. That trio removes essentially all zoom-out detail loss and popping.

---

## 6. Bad ideas / keep-these / caveats

- **Do NOT remove frustum culling** (`llspatialpartition.cpp:1062`) — off-screen geometry must stay culled or the whole scene tries to draw; even a 5090 will choke and it changes nothing visible.
- **Occlusion (Lever E) is optional, not default-off** — fully disabling HZB in dense sims can multiply draw cost heavily and can *reduce* framerate enough to cause its own capture stutter. Leave it on unless a specific shot flickers.
- **Force-LOD-3 globally is VRAM/geometry heavy** — every mesh loads its highest LOD. On a 32GB 5090 in a dense region this is fine, but it will spike VRAM and triangle counts; keep it a filming toggle, not an always-on default, and it should turn itself off (or warn) on lower-VRAM machines. Also forces mesh-repo to fetch high LODs (`llmeshrepository.cpp` uses `sLODFactor`), adding load hitching when first enabled — enable *before* rolling camera.
- **Popping is intrinsic to the LOD-change rebuild** (`updateLOD` → `markRebuild`, `llvovolume.cpp:1640`). Forcing a constant `cur_detail` is what actually stops it; half-measures (just raising the factor) only push the pop farther out, they don't remove it.
- Raising `MAX_LOD_FACTOR` also affects the near-boost ramp (`:1555`) and `sDistanceFactor` (`:400`, `1 - sLODFactor*0.1` goes negative above factor 10) — if you uncap the factor, **also revisit that `sDistanceFactor` formula** or clamp it, or distance math inverts. Flagged: this is the one non-obvious landmine in Lever A.1.

---

## 7. Black Dragon comparison

- BD's `calcLOD` / `computeLODDetail` are **functionally identical** to ours, same `IgnoreFOVZoomForLODs` handling (`black-dragon/indra/newview/llvovolume.cpp`, FOV term present) and the **same `MAX_LOD_FACTOR = 4.0`** cap (`black-dragon/indra/newview/llvovolume.h:57`). No uncapped LOD there.
- BD ships a "Machinima" sidebar panel (`black-dragon/indra/newview/skins/default/xui/en/panel_machinima.xml`, built in `bdsidebar.cpp:54`) — but it is **shadows / volumetric lighting / SSR / DoF**, NOT LOD or anti-cull. No forced-LOD, no cull-disable, no small-object toggle.
- **Conclusion: nothing to port from BD for this feature.** The anti-cull/high-LOD machinima mode is net-new work in our fork; the code sites in §5 are all in stock-derived files we already own.

---

## 8. TL;DR for implementer
1. Add setting `MachinimaHighLOD` (+ sub-settings), default off, with a Lightbox LOD-tab checkbox.
2. **Lever A.2**: in `calcLOD()` (`llvovolume.cpp:1577`) add `else if (force) cur_detail = 3;` — kills zoom-out LOD loss and popping in one shot.
3. **Lever B**: mode drives `RenderAvatarMaxNonImpostors=0`, `RenderAutoMuteSurfaceAreaLimit=0` — keeps distant avatars full.
4. **Lever C**: relax/scale `FORCE_SIMPLE_RENDER_AREA`/`FORCE_CULL_AREA` (`llvovolume.cpp:97-98`) and push `RenderFarClip`.
5. If uncapping `MAX_LOD_FACTOR` (`llvovolume.h:57`), also fix `sDistanceFactor` (`llviewercontrol.cpp:400`).
Effort: core toggle (A.2+B) ~0.5 day; full bundle with UI + FORCE_* plumbing + cap fix ~1–1.5 days.
