# Upstream Alchemy Merge — Deep-Research Plan for GPT

**Deliverable requested of GPT:** a concrete, staged **merge playbook** that lands upstream
Alchemy's large rendering overhaul into the Alchemy-Machinima fork **without breaking the two
marquee subsystems — entity clones (ghosts) and the virtual camera (Prism/VCam).** This document
is the map + the questions; GPT does the deep code reading and produces the playbook.

Repo: `I:\alchemy-machinima` (work here; sessions start in `I:\enve`). Do NOT build (Claude builds).
Do NOT `git merge` anything as part of research — this is analysis only.

---

## 0. Ground truth (measured 2026-08-10, verify before trusting)

| Fact | Value |
|---|---|
| Fork branch | `develop` (local) |
| Upstream target | `alchemy-upstream/develop` @ **`af0f3bd1beb`** (AlchemyViewer/Alchemy.git) |
| Merge-base (last sync) | **`7c11d3f38bd`** "Add support for semaphore and memory object interop for dx12/vulkan to gl" |
| Fork ahead / upstream ahead | **325** fork commits / **255** upstream commits |
| Upstream churn since base | **502 files, +22,357 / −9,418** |
| Files BOTH sides changed (conflict-prone) | **80** |

Reproduce:
```bash
MB=7c11d3f38bd2f28736f8ede94487c9bb9ffb1e8a
git fetch alchemy-upstream
git rev-list --left-right --count develop...alchemy-upstream/develop
git diff --shortstat "$MB"..alchemy-upstream/develop
git diff --name-only "$MB"..develop | sort > fork_changed.txt
git diff --name-only "$MB"..alchemy-upstream/develop | sort > up_changed.txt
comm -12 fork_changed.txt up_changed.txt   # the 80
```

> **Branch check (do first):** confirm `alchemy-upstream/develop` is the "huge rendering update"
> the user means. Candidate feature branches also exist (`darl/atlasaurus`, `rye/hb-gpu-font`,
> `archive/newmain`, `main`). If the intended target is one of those, re-run §0 against it; the
> rest of this plan's structure still applies.

---

## 1. The single most important finding

**The fork's marquee features are in fork-only files that upstream never touched** — so they will
**not textually conflict**, but they **hook deep into upstream files that were rewritten.** Verified
0-line upstream churn on all of:

```
llprismlens.cpp/.h   llcinematiccamera.cpp/.h   aldirectorswitcher.cpp
alprismcamdriver.cpp   llghostavatar.cpp/.h   llcontrolavatar.cpp
```

Therefore the merge risk is **NOT** git conflict resolution on these files. It is **semantic /
API-contract breakage**: these features call into `pipeline.cpp` (fork has **15,485** custom lines
there), `llvoavatar.cpp` (661 upstream lines changed), `lldrawpoolavatar.cpp`, `llrendertarget.h`,
the reflection-probe system, and the shader/sampler layer — **all of which upstream re-architected.**

The playbook must be organized around **protecting contracts**, not just resolving diffs.

---

## 2. What the upstream update actually IS (themed inventory)

255 commits; representative SHAs below (`git show <sha>`). Each theme is tagged with the fork
subsystem it threatens and a risk level.

### T1 — Texture / sampler / render-target architecture rewrite ⚠️ VERY HIGH
The deepest change. Textures are no longer bound through texture-unit state; render targets get
immutable storage.
- `6ff2cddb89c` Retire the texture unit: **LLTexUnit becomes ALTextureSlot**
- `78eff87b09b` Delete the mutable texture path, take the resource out of the sampler
- `bcdcda3a082` Sample through **sampler objects** instead of texture state
- `557ecd2eb17` Name the sampler at every bind site
- `f07c65fd295` Give **render targets immutable storage**, size uploads from live geometry
- `392129c0341` Allocate every LLImageGL texture with immutable storage
- `39c1cab7c68` Take raw GL texture allocation out of newview
- `a1a3933f1c6` LLImageGL thread-safe; `09b3b94a998` raise GL floor to 4.1
- Files: `indra/llrender/llrender.*`, `llrendertarget.h`, `indra/newview/llviewertexture.*`
- **Threatens:** VCam render-to-texture (`mPrismLensOutput[]` 1024² RTs, the camera-feed RT,
  projector-cookie feed swap), ghost palette/impostor snapshots, the visible-diffuse ReShade
  sidecar, 10-bit RT format work. Any fork code that allocates a render target or binds a texture
  by unit is on the old contract.

### T2 — Reflection probe overhaul (SH projection) ⚠️ HIGH
- `59601e8eba7` **Replace probe irradiance cubemap with SH projection**
- `3e6ec2e3f41` Fix probe ordering, lifecycle, undefined weights
- `c343b4a3b93` Stop probe occlusion latching / leaking into captures
- `452975ab7ff` Stop irradiance map collapsing to whole-sphere average
- `4106e14484 2` Keep probe neighbour graph current, close NaN taps; `fe2a258` priority; `ec2702c` PBR corrections to legacy
- Files: `reflectionProbeF.glsl` (234↑), probe C++ in `pipeline.cpp`
- **Threatens:** VCam **auxiliary reflection probe** (`activatePrismAuxiliaryProbeState` in
  pipeline.cpp; the known "VCam feed PBR flicker = cube-snapshot ambscale leak into aux probe"
  fix). The aux-probe containment the fork relies on may not exist in the new SH pipeline.

### T3 — Shadow filtering overhaul ⚠️ HIGH
- `e8b487a8160` **Modern shadow filtering: gather PCF, receiver-plane bias, PCSS tier**
- `f8085ab153c` Run probe captures at lowest shadow tier without lying about the sampler
- Files: `shadowUtil.glsl` (614↑), `spotLightF.glsl` (67↑)
- **Threatens:** VCam **projector shadows in the feed** (dedicated `mPrismSpotShadow[]`,
  "byte-identical-main gate"), projvol hero-shaft PCF sampler reuse, weather rain occlusion.

### T4 — REVERSE_Z depth convention ⚠️ HIGH
- `ab2d54b1b8d` **Inject REVERSE_Z at the compile choke point**, not through sGlobalDefines
- **Threatens:** every fork shader/CPU path doing depth math — projector-shadow depth, weather
  occlusion depth (`weatherRainExposure` clip.z), projvol froxel depth, VCam depth-of-field.

### T5 — Shader system + UBO refactor ⚠️ HIGH
- `f359c7f5471` Build rigged variants through `createShader`; `cd4bb5c` halve material arrays
- `0ade53ca050` Share forward light arrays through a **UB_LIGHTS** block; `036c737` UBO for shadow/SSAO/SSR
- `98ec429fcb7` Remove GLTF shader-variant batching; `e5d05ad` drop `mShaderList`
- `30221e9ba76` Add `hasUniform`; `5116b18` complete uniform type table; `0c94ec5` **remove unused reserved uniforms/globals**
- Files: `llviewershadermgr.*` (1025↑ / 726 fork), `indra/llrender/llglslshader.*`, `llshadermgr.*`
- **Threatens:** ALL fork custom shaders (`prismLensF/V`, `actorghostF/V`, `weatherRain/Surface/Lightning`,
  projvol) that reference now-removed uniforms/globals or expect the old light arrays; fork's own
  `llviewershadermgr` program registrations for VCam/ghost/weather shaders.

### T6 — PBR / GLTF BRDF corrections ⚠️ MED-HIGH
- `12dc96aa56f` Correct GLTF PBR BRDF + integration table; `ec2702c` 4 PBR corrections to legacy
- `9517e0af79e` Close punctual lighting gaps; `ed2558c` AO inside PBR lighting; `af0f3bd` Blinn-Phong lobe evaluated not textured; `03c0860` light legacy alpha like the opaque behind it
- Files: `pbralphaF.glsl`, `alphaF.glsl`, `lldrawpoolmaterials.cpp`, `lldrawpoolpbropaque.cpp`
- **Threatens:** VCam feed PBR appearance, clone PBR parity, the forced-mask-cutoff `!gltf_mat`
  exclusion logic in `llvovolume::registerFace`, interleaved-alpha PBR paths.

### T7 — Impostor bake correctness ⚠️ MED-HIGH
- `cda2d4fb0c1` Fix impostor bake: material data, sorting, normals, state
- `0ef9c3d7f53` Carry real coverage in impostor alpha, stamp in right order
- **Threatens:** clone/ghost impostor rendering + the "clone attach-float / imposter gap" work; the
  interleaved-alpha `sImpostorRenderAlphaDepthPass` depth-write path.

### T8 — Rigged-mesh skinning avatar-local ⚠️ MED
- `aff85c50ddb` Keep rigged skinning avatar-local to kill altitude precision noise (`objectSkinV.glsl`)
- **Threatens:** clone rigged mesh, the 100x/150x clone-scale **outer render transform**
  (`resolve_outer_transform` in llvovolume), clone attach-path scale.

### T9 — Draw-path / pool refactors ⚠️ MED (mostly textual)
- Bump/materials/terrain/tree/simple/water pool changes, `lldrawpool.*`, `llface.cpp`
- **Threatens:** the fork's `AttachmentFilter` 3-pass + interleaved alpha in `lldrawpoolalpha`, the
  visible-diffuse sidecar in `renderAlpha`, `registerFace` mask-cutoff, clone LOD/pixel-area work.

---

## 3. The three risk layers (frame every finding as one of these)

1. **Textual conflict** — the 80 both-changed files. Mechanical but concentrated: `pipeline.cpp`
   (15,485 fork lines vs 1,533 upstream) is the worst; also `llviewershadermgr.cpp`,
   `llvoavatar.cpp`, `llvovolume.cpp`, `lldrawpoolalpha.cpp`, `llrender/*`.
2. **API-contract breakage** — fork-only feature files compiling against changed signatures/semantics
   (render targets, sampler binding, probe API, shader uniform set, REVERSE_Z). **This is the layer
   that silently breaks clones + VCam even after conflicts "resolve."**
3. **Shader-interface drift** — fork custom shaders referencing removed uniforms/globals or the old
   light/UBO layout. Compiles-but-wrong or fails-to-link.

---

## 4. Fork assets to protect (with anchors)

### 4A. Virtual camera (Prism / VCam)
- `llprismlens.cpp/.h` — capture/display registry, `mPrismLensOutput[slot]` (1024² retained RTs),
  `renderAuxiliaryView()` (prepare-all → `chooseRenderSlot` → `markProduced`), Kooima off-axis,
  virtual (prim-free) cameras. **Depends on: T1 (RTs), T2 (aux probe), T3 (projector shadows), T5 (prism shaders).**
- `llcinematiccamera.cpp/.h` — `EMode`, `isActive()` owns the main render camera, `computeOutputFrame`/
  `writeMainCamera`/`writeVirtualCameraOutput`.
- `aldirectorswitcher.*`, `alprismcamdriver.*` — switcher-drives-VCam, follow/orbit/lock-on driver.
- pipeline.cpp hooks: `activatePrismAuxiliaryProbeState`, `mPrismSpotShadow[]`, projector cookie feed
  swap in `setupSpotLight`/`setupSpotLightVolumetric`.
- Shaders: `prismLensF/V.glsl`, projector cookie remap in `deferredUtil.glsl` (fork added
  `proj_cookie_region/orient`, `projCookieUv`).

### 4B. Entity clones (ghosts)
- `llghostavatar.cpp/.h` — clone appearance/attachment cloning, animation mirror
  (MIRROR/DIRECTED/FROZEN, `synchronizeCloneAnimations`, per-linkset animesh via separate
  `LLControlAvatar`s, the new `GhostMirrorHoldOnSourceChange` hold-on-source-change fix).
  **Depends on: T7 (impostors), T8 (rigged skinning), T2/T6 (deferred/PBR parity — GhostDeferred), llvoavatar contract.**
- `llcontrolavatar.cpp` — animesh control avatars (upstream did NOT change it; but its base/callers did).
- `llvoavatar.cpp/.h` (661↑ upstream) — clone base class; fork hooks:
  `updateRootPositionAndRotation` ghost hover/bbox skip, `markMoved` attachment fix, `idleUpdateMisc`.
- `llvovolume.cpp` `registerFace` (240↑ upstream / 208 fork) — clone rig, outer render transform,
  scale-aware LOD/cull/pixel-area, mask-cutoff.
- Shaders: `actorghostF/V.glsl` (fork-only; T5 drift risk).

### 4C. Secondary rendering features that also intersect (don't lose these)
BDMerge alpha attachment sort + **interleaved alpha** (lldrawpoolalpha, uncommitted), forced-mask
cutoff (llvovolume registerFace, uncommitted), projvol/weather shaders, projector shadows,
visible-diffuse ReShade sidecar (lldrawpoolalpha renderAlpha + pipeline), 10-bit/ReShade.

> **Uncommitted work in the tree right now** (must be preserved through the merge): interleaved-alpha
> port (PR #5927), mask-cutoff presets, ghost anim hold-on-source-change. Backup branches:
> `backup-pre-interleaved-alpha`, `backup-pre-ghost-anim-fix`. Consider committing these before the
> merge so they have stable SHAs to re-land.

---

## 5. Research phases (what GPT should execute)

**Phase A — Contract diff (the core work).** For each upstream theme T1–T9, read the representative
commits and produce a **contract-change table**: old API/uniform/semantic → new one → every fork call
site that uses the old form (grep the fork for it). Prioritize T1, T2, T3, T5 (they gate VCam) and
T7, T8, T2/T6 (they gate clones). Output: `MERGE_CONTRACT_DIFF.md`.

**Phase B — Per-subsystem adaptation spec.** For VCam (4A) and clones (4B), walk each fork file and
classify every upstream dependency as: (a) unchanged, (b) mechanical rename (e.g. LLTexUnit→ALTextureSlot),
(c) semantic re-port (e.g. render-target immutable storage, SH probe), (d) blocked/unknown. Produce a
concrete edit list per file. Output: `MERGE_VCAM_ADAPTATION.md`, `MERGE_CLONES_ADAPTATION.md`.

**Phase C — pipeline.cpp strategy.** The 15,485-line fork divergence in one file is the crux. Decide:
re-apply fork hunks onto upstream pipeline.cpp as grouped patches (by feature), or reverse-integrate
upstream's 1,533 lines into the fork file. Enumerate the fork's pipeline.cpp feature blocks (they are
comment-tagged: `[BDMerge …]`, Prism/VCam, ghost, projvol, ReShade sidecar) and map each to the
upstream region it now lives in. Output: `MERGE_PIPELINE_PLAN.md`.

**Phase D — Shader drift audit.** For every fork custom shader (§4 list) and every fork-modified
shared shader, diff the uniform/global set against upstream's post-refactor set (`0c94ec5`,
`0ade530`, `036c737`, `30221e9`). Flag removed uniforms, UB_LIGHTS/UBO migration, REVERSE_Z depth
math. Output: `MERGE_SHADER_DRIFT.md`.

**Phase E — Merge strategy + sequencing.** Recommend one of the strategies in §6, then produce the
ordered landing sequence (dependency order: `llrender` core → shader mgr → pipeline → drawpools →
avatar/volume → feature files → shaders → settings/UI), with a git-mechanics recipe and a rollback
point per step. Output: `MERGE_PLAYBOOK.md` (the top-level deliverable).

---

## 6. Merge strategy options for GPT to evaluate

- **Option A — `git merge alchemy-upstream/develop` into develop.** One giant resolution; the
  pipeline.cpp / shader-mgr conflicts are enormous and API breakage is invisible to git. *Likely
  rejected*, but state why.
- **Option B — rebase fork's 325 commits onto upstream.** 325 conflict rounds; brutal, discards the
  curated feature history value. *Reject.*
- **Option C — re-port (recommended default to evaluate).** Take upstream `develop` as the **new
  base**; re-land fork features as curated, subsystem-grouped patches, each **adapted to the new
  contracts**, in dependency order. Matches how the fork was built (feature briefs) and is the only
  option that forces the API adaptations to be done deliberately. Slowest, safest, most controllable.
- **Option D — hybrid.** Fast-merge the mechanical/settings/UI/non-render intersection (the §2C list),
  then re-port only the rendering-critical features (VCam, clones, projvol, BDMerge alpha, sidecar,
  projector shadows) against the new APIs per Phases B–D.

GPT should pick C or D with justification and encode it in `MERGE_PLAYBOOK.md`.

---

## 7. Targeted research questions (must be answered in the deliverables)

**VCam**
1. Does the fork's `renderAuxiliaryView` render-target allocation survive T1's immutable-storage RTs?
   What is the new allocation/resize API and where must `mPrismLensOutput[]`/feed RT move to it?
2. Does `activatePrismAuxiliaryProbeState` still have a probe state to contain after T2's SH-projection
   rewrite? What replaces the cube-snapshot ambscale path the PBR-flicker fix depended on?
3. Are the fork's projector-cookie uniforms (`proj_cookie_region/orient`) and `mPrismSpotShadow[]`
   compatible with T3's PCF/PCSS rewrite and T4 REVERSE_Z spot-shadow depth?
4. Do `prismLensF/V.glsl` reference any uniform removed by T5? Do they need UB_LIGHTS/REVERSE_Z updates?

**Clones**
5. Does T7's impostor bake rewrite change the surface the ghost/clone impostor + deferred parity
   (GhostDeferred) build on? Re-validate the clone attach-float / imposter-gap fixes.
6. Does T8's avatar-local rigged skinning interact with the clone **outer render transform**
   (100x/150x scale)? Does `resolve_outer_transform` still compose correctly?
7. Does `llvoavatar.cpp`'s 661-line upstream change touch `updateRootPositionAndRotation`,
   `idleUpdateMisc`, `markMoved`, or the signaled-animation members the clone mirror reads?
8. Do `actorghostF/V.glsl` survive T5? Does the ghost alpha-multiply (`ghostUseVertexAlpha`) path
   still bind?

**Cross-cutting**
9. Which fork `settings.xml` / `settings_alchemy.xml` keys collide with upstream additions? (both
   files are in the intersection).
10. `featuretable*.txt` (all three in intersection) — do upstream feature-table changes disable or
    gate any renderer path the fork's features require?

---

## 8. Deliverables (what GPT returns)

1. `MERGE_CONTRACT_DIFF.md` — Phase A table (old→new API/uniform/semantic → fork call sites).
2. `MERGE_VCAM_ADAPTATION.md` — per-file edit list for VCam against the new base.
3. `MERGE_CLONES_ADAPTATION.md` — per-file edit list for clones.
4. `MERGE_PIPELINE_PLAN.md` — pipeline.cpp feature-block re-application map.
5. `MERGE_SHADER_DRIFT.md` — shader uniform/UBO/REVERSE_Z audit.
6. `MERGE_PLAYBOOK.md` — **top-level:** chosen strategy, ordered landing sequence with per-step
   rollback points, git recipe, and a validation checklist.

Each finding tagged with its risk layer (§3: textual / API-contract / shader-drift) and a confidence
level. Cite SHAs and `file:line`. Where a fork feature cannot survive without redesign, say so
explicitly rather than hand-waving.

---

## 9. Validation plan (encode in the playbook)

Per-subsystem in-world acceptance after each landing step (Claude builds Release, user tests):
- **VCam:** monitor feed renders; adaptive-FPS; 3-lens magnifier; projector shadows in feed; switcher
  drives VCam; no PBR flicker on the feed.
- **Clones:** clone appearance + rigged + animesh + PBR parity; 100x/150x scale; MIRROR hold-on-source-change;
  impostor transitions; no LOD flicker.
- **Regression sentinels:** interleaved alpha, mask cutoff, projvol beams, weather, ReShade 10-bit sidecar.

Build reminder: `cmake --build "I:\alchemy-machinima\build-Windows-vs2026-os" --config Release --target alchemy-bin`
(PowerShell; exit-1 with 0 `error C`/`error LNK` is the benign packaging step; viewer must be closed → LNK1104).

---

## 10. Appendix — the 80 conflict-prone files

**Rendering C++ (both sides, highest priority):** pipeline.cpp/.h, llviewershadermgr.cpp/.h,
llvoavatar.cpp/.h, llvovolume.cpp/.h, lldrawpoolalpha.cpp/.h, lldrawpool.cpp/.h, lldrawpoolavatar.cpp,
lldrawpoolbump/materials/pbropaque/simple/terrain/tree/water.cpp, llface.cpp, llviewertexture.cpp/.h.

**llrender core (T1/T5):** llglslshader.cpp/.h, llrender.cpp/.h, llrendertarget.h, llshadermgr.cpp/.h.

**Shaders (both sides):** objectSkinV, deferredUtil, fullbrightF, shadowUtil, tonemapUtilF, alphaF,
pbralphaF, reflectionProbeF, spotLightF, underWaterF, waterF.

**Mechanical (settings/menus/build/misc):** settings.xml, settings_alchemy.xml, featuretable{,_linux,_mac}.txt,
CMakeLists.txt, viewer_manifest.py, menu_viewer.xml, strings.xml, floater_preferences_graphics_advanced.xml,
textures.xml, + llagent, llappviewer, llenvironment, llviewercamera, llviewercontrol, llviewerdisplay,
llviewermenu, llviewermessage, llviewerobject, llviewerwindow, llworld, llselectmgr, llmeshrepository, etc.

**Fork-only feature files (0 upstream churn — adapt, don't merge):** llprismlens, llcinematiccamera,
aldirectorswitcher, alprismcamdriver, llghostavatar, llcontrolavatar; shaders prismLensF/V,
actorghostF/V, weatherRain/Surface/Lightning/RainUpsampleF.
