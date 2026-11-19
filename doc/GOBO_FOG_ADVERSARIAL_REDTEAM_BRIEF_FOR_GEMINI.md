# ADVERSARIAL RED-TEAM BRIEF — Cinematic Gobo + Localized Fog (for Gemini)

**On disk:** `I:\alchemy-machinima\doc\GOBO_FOG_ADVERSARIAL_REDTEAM_BRIEF_FOR_GEMINI.md`
**Your job:** Design-level adversarial review, NOT implementation. Codex is writing this feature right now against a grounded spec; the code does not exist yet. Predict, at each seam below, the specific bugs Codex is likely to introduce and the exact guard each needs. Output a **prioritized findings list** (P0 build-breakers → P1 correctness/regression → P2 perf → P3 polish), each finding keyed to a file:line seam with a concrete failure scenario and the minimal fix. I will cross-check your list against Codex's actual diff.

**Decisions already made (do NOT relitigate):**
- Gobos are **procedural in-shader** for v1. NO texture-library / PNG assets / `viewer_manifest` / `LLViewerFetchedTexture` presets this pass. Ignore that path.
- Animation clock = `LLPresentationTime::currentFrame().presentation_time` (wrapped `fmod 3600`).
- In-world drag gizmo is optional; numeric fields + wireframe overlay is the shippable floor.
- Both features default OFF and must be byte-identical to today when off.

## VERIFIED FACTS (use these; do not invent anchors)
- **Shared cookie sampler:** `class1/deferred/deferredUtil.glsl` — UV = `proj_tc.xy` from `clipProjectedLightVars(...)` (~168–185). All `projectionMap` reads funnel through `getTexture2DLodDiffuse`/`getTexture2DLodAmbient`/`texture2DLodSpecular` → `textureLod(projectionMap, tc, goboLod(tc,lod))` (~226/242/289). This file is class1 but shared by ALL classes via the fallback loader.
- **Surface pass:** `class3/deferred/spotLightF.glsl` calls `getProjectedLightDiffuseColor(l_dist, proj_tc.xy)` (~186/219) + `getProjectedLightAmbiance` (~195/227).
- **Volumetric beam:** `class3/deferred/projectorVolumetricF.glsl` does NOT call `getProjectedLightDiffuseColor`; it uses a LOCAL copy `projGoboTexture(l_dist, proj_tc.xy)` (~272–278) → `getTexture2DLodDiffuse`. Call sites ~554/753.
- **Froxel injection:** `class1/deferred/froxelInjectF.glsl` — VERIFY whether it samples the projector cookie at all (does it call `getProjectedLightDiffuseColor`/`getTexture2DLodDiffuse`, or only sample the light color?). This determines whether gobo animation must reach a THIRD consumer. State the answer explicitly.
- **Per-projector int-uniform precedent:** `gobo_aniso` — decl `deferredUtil.glsl:~73`; enum `GOBO_ANISO` `llshadermgr.h:~560`; string `llshadermgr.cpp:~1761`; pushed `pipeline.cpp:~17525`. Uniform enum ordinal indexes the `mReservedUniforms` push_back vector — order is load-bearing.
- **Uniform push sites:** `setupSpotLight` `pipeline.cpp:~17358`, `setupSpotLightVolumetric` `~17537`.
- **Froxel media density:** `class1/deferred/froxelMediaF.glsl` — per-froxel world pos `wpos` (~94); density folded into scalar `sigma_t` (base ~97, noise ~107–110, height ~117–119); `sigma_s = sigma_t*albedo` (~124), `frag_color=vec4(sigma_s,sigma_t)` (~126). Local fog term inserts ~121; tint requires splitting `sigma_s` from grey.
- **Froxel driver:** one function `LLPipeline::renderFroxelVolumetrics()` `pipeline.cpp:~13503`; MEDIA block ~13583–13642 (upload point after `FROXEL_TIME` ~13634); passes P1 media→P2 inject→P3 temporal→P4 integrate→P5 apply. Master gate ~13514 `BDMergeFroxelVolumetrics && !gCubeSnapshot`.
- **Temporal blend:** `class1/deferred/froxelTemporalF.glsl` — 6-neighbor clamp ~154–163, blend `w=clamp(froxel_temporal_blend*validity,0,0.95); outc=mix(cur,hclamp,w)` ~169–170. Reference guard already in tree: `projectorVolumetricTemporalF.glsl:180–186` (`w*=exp(-reject*change)`). NOTE: froxel temporal blends the LIGHT atlas, not the media atlas; a static fog box (deterministic media) does not ghost, but an ANIMATED/moving one perturbs injected light `cur` which IS blended.
- **Array upload precedent:** `uniform4fv(PROJVOL_FRUSTUM_PLANES, 6, ptr)` `pipeline.cpp:~17632`. GLSL has no struct-array uniforms — flatten to `vecN[N]` + `uniform*fv(loc,N,ptr)` + `uniform1i(count)`. Cap N=8.
- **Manager analog:** `ALDirectorAnimSwitcher` (Meyers singleton, LLSD `loadBank`/`saveBank`, per-field `sanitize`), ticked `llappviewer.cpp:~5518` with `presentation_time`. New `ALLocalFogManager` follows this; must NOT include floater/XUI headers.
- **Overlay:** `ALWorldOverlayViz::ScopedRenderer` (depth-off, agent-metre coords); NO box/ellipsoid primitive — compose from `drawSegment`/`drawRing`. Hook at `llviewerdisplay.cpp:~1826`.
- **Manip proxy:** `ALGhostManipProxy` (invisible local `LLVOVolume`, `LOCAL_OBJECT_GHOST_MANIP_PROXY` `llviewerobject.h:~857`), ticked `llappviewer.cpp:~5503`.

## SEAMS TO RED-TEAM (produce findings for each)
1. **Uniform-registration lockstep** — enumerate the exact failure if a new enum is added to `llshadermgr.h` without inserting its `push_back` at the matching index in `llshadermgr.cpp` (silent uniform aliasing → wrong data). What's the detection test?
2. **Shared-helper drift** — the beam uses `projGoboTexture`, the surface uses `getProjectedLightDiffuseColor`. If animation is added to only one path, slats desync. Which single function must host `sampleGobo` so all consumers inherit? Confirm froxelInject's involvement (fact above).
3. **Froxel temporal ghosting** — precise conditions under which an animated fog volume smears; verify the `exp(-reject*change)` guard sourcing `change` from `froxelMedia.a` neighbor deltas is correct; atlas tile-clamp: do the neighbor fetches wrap across grid edges/slices (bleed)? Give the clamp guard.
4. **SDF / matrix / quaternion numerics** — non-unit `LLQuaternion` → non-orthonormal `inv_rot`; rotation-matrix inverse should be transpose (build from a NORMALIZED quat). Div-by-zero in ellipsoid `p/extents`, feather `smoothstep` denominators, tint normalize `max(_,eps)`. List each with the guard.
5. **Per-froxel perf** — cost of N=8 volumes × fbm turbulence × ~900k froxels; is the AABB early-out actually cheap/branch-coherent? Voronoi caustics gobo cost in the beam march (per-step) vs surface (per-pixel) — flag if Codex puts multi-tap/Voronoi in the volumetric march.
6. **Off-path parity** — prove `gobo_pattern=-1` + `localfog_count=0` + `reject=0` yields byte-identical output; any uninitialized uniform (e.g. `gobo_optics.w` sim_time) that perturbs the off path?
7. **Cross-thread** — `ALLocalFogManager` ticked in idle (`llappviewer` main thread) but read during render (`pipeline.cpp` render). Any data hazard if the array is rebuilt while the render reads it? (Single-threaded render submission here, but confirm.)
8. **REVERSE_Z / depth** — this fork uses REVERSE_Z in places; does the overlay cage (depth-off) or any fog depth compare assume standard Z? Flag if relevant.
9. **Manip-proxy routing (if gizmo attempted)** — reusing the ghost proxy needs a distinct `LOCAL_OBJECT_FOG_MANIP_PROXY` kind or selectmgr will misroute; non-uniform box extents need 3-axis stretch mapping. Failure modes if Codex reuses the ghost enum verbatim.

## OUTPUT FORMAT
A single prioritized list. For each finding: `[P0..P3] <seam#> file:line — failure scenario — minimal fix`. No prose intro, no restating this brief. If a seam is fine, say so in one line. Do NOT write code or `.md` files; return the list as your message.
