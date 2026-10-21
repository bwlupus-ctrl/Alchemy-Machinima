# Relight / Second Life integration audit and ranked plan

2026-10-20. Branch `feature/director-kit-p0`, baseline HEAD `8d22204597d0`.
Read the complete quality brief and both phase reports; the reports' later
runtime, housing, camera-alignment and workload-budget corrections supersede
their original implementation descriptions. References below are to the current
worktree. `NV/` means `indra/newview/`; `GLSL/` means
`indra/newview/app_settings/shaders/`. These are repository findings, not claims
about other viewers or the Second Life service. Visual consequences explicitly
marked **inference** require in-world validation.

Only Tier A was implemented. No shader, lighting pipeline, native shadow policy,
G-buffer, bridge ABI or primitive clamp was changed. No commit, push, stash,
viewer build or viewer launch. Existing untracked work and dirty `vcpkg` remain.
`.tmp/ref_private/` was not opened. No external reference implementation was used.

## 1. Native shadows and the Relight hybrid

### Directional sun/moon

- `NV/pipeline.cpp:30472` gates native generation on deferred rendering and
  `RenderShadowDetail > 0`. At `:30580` the caster direction selects sun or moon
  using the environment's sun-up state. This is a directional source: there is
  no finite prim-centre origin to align with a local-light ray.
- `NV/pipeline.cpp:30705` computes distance splits; `:30959` iterates four main
  view cascades (two in cube captures). This fork also has an optional subject
  column in cascade 0 (`:30974`), orthographic fitting by default with an optional
  perspective warp (`:31415`), and per-cascade coefficients (`:31589`).
- `GLSL/class1/deferred/shadowUtil.glsl:59` has four sun matrices followed by
  ten projector matrices. Directional sampling blends cascade bands; legacy
  receiver displacement is along L (`:791`), while world-unit mode uses
  millimetre depth bias and texel-scaled normal displacement (`:74`, `:874`).
  Consequently neither sun bias setting is directly interchangeable with the
  screen trace's world-space receiver bias.
- `GLSL/class3/deferred/softenLightF.glsl:175` chooses the same sun/moon light
  direction. At `:182` the light buffer R channel supplies directional visibility,
  G supplies SSAO; `:193` retains the native base-alpha shadow exception.
  `:244` passes visibility into PBR base lighting. Legacy direct/ambient and
  reflection composition follows at `:280`.
- Forward legacy alpha samples the directional map at
  `GLSL/class2/deferred/alphaF.glsl:328` and uses the native `min(final_da,shadow)`
  convention at `:455`. Forward PBR samples it at
  `GLSL/class2/deferred/pbralphaF.glsl:314`, then passes `scol` to base lighting
  at `:384`. These directional conventions do not mean that local-light
  visibility should multiply the entire base/ambient image.

Relight changes local direct lighting only. It does not soften or re-render
sun/moon maps. **Inference:** local disk penumbrae can read differently next to
sun shadows because source models, fill floors, biases, footprints and blocker
coverage differ. A global common softness slider would hide these differences,
not establish a physically common source size.

### Projector slots, matrices, offsets and kernels

- `NV/pipeline.cpp:28939` (`setupSpotLight`) computes the optical origin from
  prim position, render rotation, scale and FOV. In symbols, with forward axis
  `a`, prim centre `p`, scale `s`, `d=(s.y/2)/tan(FOV/2)`, it is
  `o=p+a*(s.z/2-d)`. Lighting uploads `proj_origin` at `:29014`.
  The radial attenuation range is still `1.5 * prim light radius`, centred on
  the light prim, rather than distance from the virtual optical apex.
- Native map generation repeats that optical-origin construction at
  `NV/pipeline.cpp:31848`, uses near `d+0.01`, far `d+1.5*radius`, and stores
  biased projection * light view * inverse camera view at `:31891`.
  Cookie projection has its existing, slightly different near/far clipping
  conventions (`:28982`); this audit does not unify them.
- `NV/pipeline.cpp:29017` resolves the *currently sampled* `mShadowSpotLight`
  slot, including fading maps. `:29047` starts the next-target auction;
  `:29089` ranks ordinary rig, world, then actor-only rig tiers, with numeric
  priority within a tier. The actor tier borrows spare slots (`:31710`).
  `BDMergeMaxSpotShadows` bounds the auction; allocation and that policy are
  off-limits. `RenderShadowDetail > 1` enables projector generation (`:31655`).
- `NV/pipeline.cpp:25124` uploads altitude-adjusted sun bias and the independent
  `RenderShadowOffset`, `RenderSpotShadowBias`, `RenderSpotShadowOffset` values.
  `GLSL/class1/deferred/shadowUtil.glsl:1023` displaces projector receivers along
  the normal by `spot_shadow_offset`; `:454` adds `0.8*spot_shadow_bias` to the
  projected depth reference. This is not the sun's legacy L-offset convention.
- `shadowUtil.glsl:450` is native projector PCF. The classic path is a small
  fixed compare kernel; the optional PCSS-lite-style path (`:464`) grows its
  kernel using projected depth, global/per-projector softness and a hard cap.
  It is not a full physical blocker-search PCSS solver. Poisson/Vogel choices
  and tap count are runtime uniforms (`NV/pipeline.cpp:25148`). A shadow fill
  floor can lift partially shadowed samples (`shadowUtil.glsl:514`). Sun soft
  kernels are separately gated by `soft_shadow_sun` (`:196`).

### Visibility convention, origin mismatch and double edges

Both systems encode **0 = blocked, 1 = lit**. The existing selected-projector
hybrid at `GLSL/class3/deferred/spotLightF.glsl:192` first applies native map
sampling and fade, then `min(map, screen)` at `:198`. It does not multiply the
two visibilities, so coincident fractional edges are not darkened twice by a
product. Both shaped and point branches receive this same `shadow` variable.
Cookie/cone evaluation remains in front of the direct lighting; authored
projector ambiance and Rig Rim retain their native exceptions (`:264`, `:285`,
`:363`, `:405`). Do not multiply whole projector output a second time.

The ordinary deferred point origin is its uploaded view-space drawable centre
(`NV/pipeline.cpp:27760`); the area gather transforms that same agent-space
centre with the current camera view (`:26842`). Forward HW local lights use
`getRenderPosition()` (`NV/pipeline.cpp:11909`) transformed by the render light
state, with spot direction from the prim rotation. They do not reconstruct the
scale/FOV-derived deferred optical apex. Shape follows each path's existing
light vector; this task does not change that forward/deferred approximation.

Points have no native local shadow map. Batched points weight the individual
light colour at `GLSL/class3/deferred/multiPointLightF.glsl:169`; per-volume
points weight the completed contribution at
`GLSL/class3/deferred/pointLightF.glsl:298`. The latter includes the existing
clamps and Rim/Actor FX composition. These are visibility weights, never
occlusion values requiring inversion. Forward local-light functions retain
their point/spot lighting without area visibility; their directional shadows
are a separate operation.

**Confirmed mismatch:** the gather upload at `NV/pipeline.cpp:26842` uses the
drawable's prim centre for *all* lights, including projectors. Native projector
shading and maps use the optical origin above. They coincide only for particular
scale/FOV combinations. The gather's receiver offset is also
`max(0.02 m,1.5*trace-pixel footprint)`
(`GLSL/class1/deferred/screenSpaceRayUtil.glsl:36`), not the native projector
normal offset/depth bias. The trace has a separate housing exclusion and
shortened endpoint (`:44`, `:65`), finite surface crossing refinement (`:70`),
and off-screen/unsupported segments remain lit.

**Inference:** `min()` takes the darker envelope of two displaced penumbrae;
it cannot align them and can produce a shoulder/double boundary. Native map
fill can also be overridden by a darker screen result. This audit does **not**
claim projector edge coincidence within one full-resolution pixel at 1080p.
The existing moving-camera test measures reconstructed screen-trace alignment
at small fixture sizes, not coincidence with a real projector map. Origin,
receiver-bias and softness unification moved to B1; changing only an endpoint
could regress the already validated housed-lamp pool.

## 2. SL prim light model and source size

| Input / path | Current behavior and interaction |
|---|---|
| Primitive clamps | `indra/llprimitive/llprimitive.cpp:74` defines radius 0-20 m and falloff 0-2. `llprimitive.h:161` clamps linear RGBA, including intensity alpha, to 0-1; `:162` clamps radius/falloff. No changes made. |
| Viewer intensity | `NV/llvovolume.cpp:3362` writes intensity through clamped RGBA; `:3445` returns linear RGB multiplied by intensity. Relight gain is separate shader data, not a bypass of the SL intensity clamp. |
| Falloff mapping | `NV/llvovolume.cpp:3601` multiplies authored falloff by a caller fudge factor; `NV/pipeline.cpp:475` sets deferred factor 0.5, so shader falloff is normally 0-1. Shader range is radius * 1.5, at most 30 m. |
| Legacy vs physical | `GLSL/class1/deferred/deferredUtil.glsl:957` retains legacy window mode or calibrated quartic-window inverse-square, with near floor and peak cap. `NV/pipeline.cpp:11753` obtains radius/reference, computes gain at `:11769`: world reference R/2, rig effective orbit reference. A cap or reference outside range can prevent calibration identity. Radius zero returns the exact point-shading branch even with physical mode selected. |
| Sphere shading | `deferredUtil.glsl:935` integrates the sphere horizon; `:947` supplies an angular ratio, avoiding a second distance falloff. `:974` splits centre diffuse and representative specular; PBR uses the existing corrected Karis normalization. Legacy gloss LUT weighting stays legacy at `:1000`. Source radius does not alter the SL influence radius. |
| World source | `NV/pipeline.cpp:11737` reads the global default, clamped 0-1 m. It does not read prim scale. At zero size, Soft shadows alone uses a 0.10 m disk fallback (`GLSL/class3/deferred/areaShadowF.glsl:47`); Light shape stays a point. This is a shadow-only artistic default, not emitting geometry inferred from SL. |
| Cookies | `NV/pipeline.cpp:28939` retains the existing projector transform and texture. `spotLightF.glsl:246` evaluates the same cookie/cone for the sphere branch. Disk visibility does not integrate/filter the cookie over its sampled aperture. Large source sizes therefore broaden shading/shadow softness without physically blurring gobos. Forward spots retain the existing cone approximation, not deferred projector-cookie fidelity (`NV/pipeline.cpp:11939`). |
| Materials | Both deferred PBR and legacy branches consume shape and selected visibility. Forward legacy alpha/material, native PBR alpha and shared Actor FX PBR consume shape. Forward `w=-1` is deliberate: opaque depth cannot describe hair in front of a wall. Legacy gets diffuse wrap and shifted LUT highlight, not PBR roughness/energy conversion. |

An opt-in **emitting-prim size** mode is feasible: the individual emitting
volume's `getScale()` is already available at `NV/pipeline.cpp:28957`. Use that
prim, not its whole linkset's bounding box, and preserve rig fixture metadata
as higher priority. But a prim can be a tiny invisible bulb, a large decorative
housing, mesh with empty bounds, an attachment or a child prim. Its bounding
box is not an authored luminous surface. B3 must choose the radius convention
(inscribed sphere, equivalent aperture, or explicit multiplier), clamp, and
whether projector XY aperture or full XYZ scale wins. Default remains global
radius zero per D1. Never infer influence radius or raise primitive clamps.

## 3. Preferences, governors and budgets

- Native Preferences control `RenderShadowDetail` is in
  `NV/skins/default/xui/en/floater_preferences_graphics_advanced.xml:860`.
  Zero disables native maps; level 1 allows directional maps, greater than 1
  allows projector maps. That setting already calls `handleSetShaderChanged`
  (`NV/llviewercontrol.cpp:1236`). No new Relight listener uses it.
- Soft shadows deliberately does not test `RenderShadowDetail`. It can stop a
  visible point-light leak while native shadows are off. At
  `NV/pipeline.cpp:27550` selection receives the effective local-light count;
  `:26621` exits for zero count. Selection intersects the actual collector at
  `:26641` without editing count-before-filter ordering. Actor-only draws have
  their independent scan and matching admission branch (`:26644`, `:27931`).
- Soft shadows is called only from deferred lighting and needs the deferred
  depth/normal/programs. It does not create shadows for the nondeferred renderer.
  Light shape's production shader coverage is the deferred renderer and its
  forward/alpha passes, not a replacement nondeferred fixed-function model.
  `NV/pipeline.cpp:11928` still distinguishes HW attenuation by renderer.
- Existing FPS AutoTune adjusts draw distance and avatar render budgets based
  on observed timing (`NV/llperfstats.cpp:425`, `:471`). Projector volumetrics
  have a separate projected-coverage adaptive sample count
  (`NV/pipeline.cpp:22522`); Prism has its own adaptive controls. None adjusts
  `RenderAreaShadow*`. Reusing FPS/VRAM pressure as Relight sample seeds or
  quality decisions would make output depend on execution history, violating §6.
- `NV/llviewertexture.cpp:629` controls the VRAM governor. Texture accounting
  already includes render-target textures (`:651`); RT bytes must not be added
  again. Fixed allocations raise the effective cap floor (`:675`); texture
  discard cannot reclaim active area targets. The governor can lower texture
  detail without reducing shadow march cost. System-memory texture/mesh budgets
  (`NV/bdmergememorybudget.cpp:222`) are another separate pressure mechanism.
- `NV/alareashadowmath.h:76` plans work using actual dimensions, selected K and
  64-bit arithmetic. Budget is 1-1500 million iterations/frame, each draw at
  most half. It reduces rays, then steps, then forces half resolution; impossible
  inputs dispatch nothing. `NV/pipeline.cpp:26764` uses this plan and splits
  channel draws at `:26870`. No timing result enters it. Presets remain requested
  values, with effective clamps logged. This bounds march iterations, not total
  memory, texture reads, denoise, GPU milliseconds or TDR risk.
- Each RGBA8 plane uses gather + scratch + full-resolution visibility. At
  1080p half resolution that is about 11.9 MiB/plane, twice for K>4; at 4K/K=8
  about 94.9 MiB total (full trace: about 189.8 MiB). Admission can reduce march
  work without reducing K/plane count. Lazy allocation is at
  `NV/pipeline.cpp:26788`, and release/resize lives beside screen targets at
  `:1376`, `:3187`. A separate memory admission policy belongs in B4/C1.

Recommendation: keep Soft shadows independent of native shadow quality for
now, explain that independence in the real card (A2), and decide any follower
mode explicitly (B4). A future quality follower should use stable requested
Preferences values and the deterministic ceiling, with a capture override;
do not introduce timing-driven degradation overnight. A warning/status is more
useful than silently overwriting the user's creative switch.

There is also an inherited capture reproducibility limit: the Relight ranking,
disk seed and work plan are deterministic **given the current drawn set and
map assignments**. Admission still intersects `mNearbyLights`, whose lifetime
uses fade state (`NV/pipeline.cpp:11344`) advanced from frame interval at
`:12045`; equal-distance collector ties use drawable pointer ordering
(`NV/pipeline.h:2057`). Native projector assignment visibility also fades with
frame interval (`NV/pipeline.cpp:31662`, `:31759`). Thus the existing synthetic
repeatability tests do not prove identical selected sets/hybrid output after
different viewer histories or process restarts. §6 full-frame checking must
include light-set/slot metadata or a settled capture state. B9 addresses this
without changing the off-limits count-before-filter or slot-auction policies.

## 4. Reflection probes and ambient

`GLSL/class3/deferred/softenLightF.glsl:234` samples probe irradiance/radiance
separately from direct local lights. PBR material roughness still controls probe
lookup; Relight does not write roughness or enlarge the probe highlight. At
`:284` legacy ambient/gloss/environment samples retain their own composition.
`NV/llreflectionmapmanager.cpp:2869` captures probe faces; cube captures are
excluded by `NV/pipeline.cpp:11731` and `:26565` for shape and visibility.
Local lights can still render in a probe according to probe level
(`NV/pipeline.cpp:27568`), using their previous lighting model.

There is no new probe-light contribution or second probe sample in Relight.
Direct BRDF lighting and reflected/ambient illumination are distinct terms.
**Inference:** captured bright surfaces can already contribute indirect light
alongside direct lights, but this audit does not prove a scene-wide energy
balance. The intentional unchanged probe captures mean the visible source's
new broad direct highlight is not guaranteed to match its reflected/baked
appearance. Legacy projector environment-texture terms remain within the
projector (`spotLightF.glsl:316`, `:462`). Do not subtract probe energy as a
small "double counting fix"; probe refresh/material/capture policy is C3.

## 5. Fork integration inventory

| Feature | Today, interaction, and disposition |
|---|---|
| Cine Light Rig | `NV/alcinelightrigmodel.h:77` bounds fixture source diameter to 0.01-4 m. Evaluated source radius is half diameter at `NV/alcinelightrig.cpp:3065`. Manager lookup `NV/alcinelightrigmanager.cpp:829` gives projectors and bounce omnis the same source metadata/reference; catchlights use global source radius. Shape and visibility use that metadata independently of their masters. No duplicate size control needed. |
| Rig Rim | `NV/alcinerigrim.cpp:251` uploads live Relight masters/limits beside Rim. Shaped direct light uses new falloff but Rim preserves its legacy attenuation/centre direction (`multiPointLightF.glsl:182`, `spotLightF.glsl:285`). The projector hybrid follows native Rim shadow relaxation; batched point colour weighting and per-volume point final weighting also shadow Rim. Existing silhouette/wrap and deliberate shadow relaxation need visual checking, not a second visibility multiply. |
| Actor Mask / actor-only | Selection already includes eligible actor-only lights when their independent draws/programs/recipient bits are ready (`NV/pipeline.cpp:26644`). Deferred draws upload shape/channel at `:27986`, `:28029`. `GLSL/class1/deferred/actorMaskGateF.glsl:10` discards nonrecipients **before** the shared light body. Thus eligible opaque actors already receive soft shadows and the receiver mask is respected; adding a new mask multiply would be redundant. The gather sees all visible opaque occluders, not only the light's recipients. Alpha remains D6; mirror lights are excluded. Whether to offer an actor-only opt-out or recipient-only blockers is B5, not an unimplemented prerequisite. |
| Mirror rig clones | `NV/alcinelightrig.cpp:1037` copies emitter state and at `:1106` copies evaluated shape metadata; manager lookup includes mirror IDs (`alcinelightrigmanager.cpp:855`). `NV/pipeline.cpp:26674` excludes mirror lights from area selection; `deferredLightShape` at `:26578` resolves only each drawable's own ID. Copies never borrow the source's channel. Preserve the clone-only/no-shadow policy. |
| Rig native shadow modes | `NV/alcinelightrig.cpp:1888` uses no-shadow flags for Off / Key / All. Context menu opt-out checks both emitting prim ID and root-edit ID (`NV/pipeline.cpp:23310`). Area selection currently does **not** consult that predicate. A suppressed projector can therefore receive a screen shadow while owning no map. This affects explicit art direction; B2 must settle whether "Cast shadows" controls both systems, and what happens to its bounce omni/catchlight. No native policy was edited. |
| Rig presets | Per-fixture source size already serializes (`NV/alcinelightrig.cpp:497`, `:608`) and `NV/alkitpresets.cpp:171` lists source size with fixture settings. `:125` owns rig creative settings. Global Relight settings are outside the rig-preset schema. Adding them implicitly would change all rigs when loading one rig. B6. |
| Rim/custom looks | `NV/alkitpresets.cpp:60` explicitly excludes master enables and performance/quality knobs from look ownership. `:74` owns only Rim prefixes; `:150` assigns the Rim page's preset scope. Relight controls sharing that page are independent features. Preserve existing look ownership; do not silently expand every Rim preset into a global relighting preset. B6 can add a separately authored optional block. |
| Director scenes / recovery | Before this change, `NV/llfloaterdirector.cpp:3581` omitted all Relight globals despite saving analogous Rim controls. **A1 implemented:** nine creative controls added at `:3712`. Save reads the allowlist at `:4075`; load checks for each present key at `:4342`; recovery signatures use the same list at `:4572`. Old scenes omit these keys and leave live controls alone. Quality, budget, light count, resolution and debug remain global. |
| Light Cues | `NV/alcinelightrig.cpp:3926` captures setup/globals/transforms, FX and per-fixture shadow softness. Source size is already in setup; it can change shape and disk radius during cue interpolation. Global Relight masters/quality are not cue data. Adding switches mid-cue or animating global work is B6; do not make a cue compete with a whole-scene quality preference. |
| Search / Quick | `scripts/code_tools/director_kit_gen.py:1350` indexes real rows, validates destinations at `:1395` and rejects coverage drift at `:1418`. A2 adds keywords to existing shape/area controls and a real readout. `NV/alkitshell.cpp:2111` builds Quick proxies from real source rows, not a separate Relight setting copy. Card header switches are real `ALKitCard` widgets but are **not dedicated row search/Quick entries** in this index loop; their child rows lead to the card. Extending header-switch discovery/proxy support is B7; do not add phantom hits or duplicate setting controls. |
| Tron / Actor FX | Forward shared PBR path `GLSL/class2/deferred/sharedActorFxPbrF.glsl:250` consumes shape and retains authored FX. Legacy material/alpha use their shape helpers; opaque FX use the G-buffer's real materials/normals. Tron emissive/post contributions are not local BRDF illumination and should not automatically be shadowed by local visibility. Mask/silhouette and Rim conventions stay as committed; no new actor-tag/G-buffer encoding. |
| VCam / Prism | `NV/pipeline.cpp:26565` rejects `sPrismLensRender`, separately from cube captures. Shape also excludes it (`:11731`). Prism native projector maps have their separate path at `:31994` and preserve that behavior. D4 excludes area shadows in v1; per-feed targets/camera/masks/budgets and restore isolation are B8. |
| Snapshots | `NV/llviewerwindow.cpp:5956` implements raw snapshots, with subimage display calls at `:6144`. `NV/pipeline.cpp:26568` rejects camera zoom factor >1 and logs tiled exclusion at `:26617`. Untiled captures use actual target dimensions, resolution-scaled sampling/filtering (`:26828`) and the hard work plan. Larger captures can reduce effective ray counts; they need not match preview noise exactly. Shape remains usable in tiled captures; Soft shadows are disabled there, so do not promise tile continuity. |
| ReShade bridge | `NV/llreshadebridge.cpp:368` exposes depth diagnostically, and `:376` explains preserved albedo/normals, supplied at `:394`; effects normally use ReShade generic depth. Relight modifies lighting, not these depth/normal sidecars, their layout or ABI. Its RGBA visibility planes are not published as bridge channels. Running an external relighter simultaneously adds another lighting treatment; compare with one system enabled at a time. Bridge modifications remain off-limits. |

## 6. Ranked implementation plan

Effort is engineering scope, not a commitment to an unattended build. Rank is
within each tier. All future renderer work retains the §6 current-frame/UUID
determinism rule, OFF identity requirement, explicit view exclusions and the
existing iteration ceilings. No proposal authorizes a §10 off-limits change.

### Tier A — safe small, implemented tonight

| Rank | Item and user-visible effect | Risk / effort / status |
|---|---|---|
| A1 | Save/restore creative Relight state with Director scenes and autosave recovery. Persist `RenderLightShapeEnable`, `RenderLocalLightPhysicalFalloff`, `RenderLocalLightDefaultRadius`, `RenderLocalLightNearFloor`, `RenderLocalLightPhysicalMaxBoost`, `RenderAreaShadows`, `RenderAreaShadowMaxDist`, `RenderAreaShadowThickness`, `RenderAreaShadowLightBias`. | Low; one allowlist change, no new load algorithm or callback. Explicit scene Load can enable the bounded feature, as loading a rig/Rim master already can. Quality controls and safety budget remain global; old sparse scenes do not reset controls. Small. **Implemented.** |
| A2 | Search for Relight, sphere source, inverse square, local/contact shadows or SL/native shadow integration reaches real controls/help. Soft shadows > More options > SL shadows explains Preferences independence and global quality/budget ownership. | Low; source-authored data, one wrapped readout, existing rows/keywords and existing navigation/proxies. No draw loop or settings write. Small. **Implemented**, row pin 2157 -> 2158 with comment. |

No shader/visibility factor correction is in Tier A: the factor convention and
receiver masks are already correct, while physical edge alignment is not a
safe data-only change. No extra shadow-pass work or shader reload is introduced.
All production render/shader files are unchanged from baseline. Feature OFF
therefore runs the same rendering code; full-viewer baseline image identity
remains an in-world evidence requirement rather than a new claimed measurement.

### Tier B — user decisions and targeted validation required

| Rank | Decision / proposed user-visible effect | Risk / effort / acceptance |
|---|---|---|
| B1 | Align projector contact traces with optical map origin; decide shared receiver normal bias vs separate screen contact bias, and whether screen visibility should respect native fill/softness. Preserve radial falloff at prim centre. | Medium/high visual risk; medium effort. Keep housing exclusion tied to physical emitter even if ray target moves. Test point/spot, rotated/scaled/child prims, FOV, alpha silhouettes, full/half resolution, Low/Cinematic, camera motion and live edits. New production-map + gather fixture must compare map-only/screen-only/hybrid at **1920x1080**, hard tiny positive source, no fill, controlled bias; require <=1 full-res pixel edge separation and one monotonic boundary, then test soft profiles separately. Include wrong-centre/wrong-bias negative controls. No map allocation/auction changes. |
| B2 | Make explicit Cast shadows opt-out and rig Off/Key/All policy govern area shadows too, or label native-only behavior. Decide whether associated bounce/catchlights should inherit suppression. | Medium semantic/visual risk; small-to-medium effort after decision. Excluding suppressed candidates before top-K would reduce work and free channels, but changes current intentional-looking lit pools and priorities. Test emitting child/root IDs, rig modes and live flag transitions, with ordinary leaking points remaining eligible. |
| B3 | Opt-in emitting-prim source radius, with fixture metadata precedence and conservative clamps. | Medium appearance risk; medium effort. Decide bounding convention, projector aperture and attachment behavior. Never change default D1 or interpret whole-linkset bounds as luminous geometry. Test tiny bulb/large housing/mesh/child/attachment and mirrored rigs. |
| B4 | Preferences follower/status and optional memory admission. Recommend independent switch with clear unsupported-view/effective-quality status first; optional deterministic quality scaling from requested native quality later. | Medium capture/performance risk; medium effort. Auto-off at native shadows 0 would remove the useful point-light leak fix. Timing/VRAM-dependent automatic quality conflicts with repeatability unless capture pins a stable configuration. Retain the current hard budget and preserve requested settings; no handleSetShaderChanged dependency. |
| B5 | Actor-only shadow policy: retain current opaque receiver support; optionally offer per-rig participation and decide whether all visible opaque objects or only recipients can occlude. | Medium visual/mask risk; medium effort. Current receiver mask already works. Recipient-only blocker depth requires a different surface source; using receiver bits to erase world occluders would leak. Do not apply opaque visibility to alpha (D6). |
| B6 | Optional Relight block for rig presets/Rim looks and cues; possibly scene quality opt-in. | Medium ownership/backward-compatibility risk; medium effort. Default preserve global masters/quality for single-rig/look loads. Define sparse old files, explicit opt-in ownership and undo. If quality is ever saved, restore preset/numeric knobs atomically so existing listeners do not overwrite custom values; never silently raise machine safety budget. |
| B7 | Give real card header enables searchable/favoritable destinations without duplicate widgets; improve Quick coverage for independent masters. | Medium framework scope; medium effort. Generator index, runtime row lookup/proxy and functional tests must agree. Existing child row discovery stays usable. No fake row naming a widget absent from the page. |
| B8 | D4: opt-in VCam/Prism area shadows with isolated per-feed camera/targets, total feed+main budgets and restoration checks. | High GPU/state risk; large effort. Excluded in v1. Requires measured multi-feed workload, mask ownership, feed resizing and no main-view channel leakage. |
| B9 | Define a capture reproducibility contract for native collector membership, projector slots/fades and governor state; record these with the take or require a documented settled state. | Medium/high integration risk; medium-to-large effort. Distinguish identical current-frame inputs from identical camera alone. Add replay tests across history/process permutations before claiming full §6 compliance. Do not reorder stock count-before-filter or alter native shadow allocation/auction in this work. |

### Tier C — later / Phase 3

| Rank | Item / user-visible effect | Risk / effort |
|---|---|---|
| C1 | Hi-Z and per-tile light selection improve long rays and many-light coverage; add deterministic CPU/GPU/memory instrumentation before increasing limits. | Medium/high renderer risk, large. Preserve global workload admission; Hi-Z construction/filtering need their own accounting and OFF cleanup. |
| C2 | D6 forward/alpha per-fragment visibility for hair and sheer cloth; optional micro-contact detail. | High correctness/work risk, large. Opaque wall visibility must never be reused for foreground hair; budget and deterministic surface identity need new tests. |
| C3 | Explicit probe/capture material consistency and physically coherent projector aperture/cookie filtering; compare direct and reflected source appearance. | High scope, large. Keep probes/hero mirrors excluded until separately reviewed; do not change G-buffer/bridge ABI. |
| C4 | Tile-safe capture through a planned full-frame depth/visibility strategy or overlap plus correct cross-tile occluder coverage. | High memory/state risk, large. A blur guard band alone cannot recover occluders outside the tile; keep tiled Soft shadows disabled meanwhile. |
| C5 | In-world calibrated quality profiles and cost reporting across GPUs/resolutions. | Medium effort/risk. Iteration bounds are not timings. Temporal reuse/ReSTIR and preview accumulation remain deferred (D3); no frame/time noise seeds. |

## 7. Implementation and validation record

A1 changes only `NV/llfloaterdirector.cpp`'s scene allowlist. A2 changes
`scripts/code_tools/director_kit/director_mockup_data.json`, the documented row
pin in `director_kit_gen.py`, and generated kit/page-source/functional outputs.
Other generated pages carry source hashes and shifted C++ owner anchors, not
additional authored lighting changes. All JSON is written with LF.
A final semantic comparison caught an initial Windows text-encoding mistake
in the JSON edit. The source was reconstructed from committed UTF-8 and only
the four keyword fields and one readout were reapplied. Exact parsed-data
comparison verifies every unrelated field, including non-ASCII labels, is
preserved. All generators/check modes and the complete prebuild guard were
rerun after that correction.

Real click path: **Machinima > Director Console (F2) > Light > Rim > Advanced**.
Independent Light shape / Soft shadows header switches remain available;
Soft shadows > More options contains the new SL shadows readout. Search hits
point to existing physical-falloff/world-size/light-count/quality widgets or
that real readout. Director scene Save/Load in the transport bar uses A1.

No shader harness cases were changed: the implemented items do not alter
shading, selection or bias. Existing production GPU, moving-camera and OFF tests
were run unchanged. The proposed native-projector coincidence/no-double-edge
fixture is explicitly B1 acceptance work, not a synthetic test falsely presented
as proof that the current hybrid already aligns.

| Required gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, exit 0: 31 production GLSL units/permutations, 44 production links; live master/off/stale data probes, 126 rig rows with zero failures, point/spot/all eight area channels, gather/filter, contact hardening and moving-camera cases. RTX 5090 / NVIDIA 610.88 / GL 4.6. `.tmp/relight_sl_shaders.log`. |
| `relight_area_check.py` | **PASS**, exit 0: budget split byte identity, housed-lamp/independent blocker, full/half, source-local rejection, deterministic gather/filter and all four presets. Camera motion at 64x48, 63x47 and 32x24 has <=1px reconstruction alignment. **1080p native-map coincidence/no-double-edge: NOT MEASURED**, deferred B1. `.tmp/relight_sl_area.log`. |
| Tests | **PASS 14/14** `alrelightmath`; standalone test TU compile/link against existing runner/libraries, no viewer target. `.tmp/relight_sl_math_compile.log`, `.tmp/relight_sl_math_link.log`, `.tmp/relight_sl_math.log`. Existing nonfatal vswhere diagnostic remains. |
| OFF/runtime preservation | **PASS**, disabled expressions match HEAD in all eight shader modules; runtime/no-reload/no-temporal checks pass. `.tmp/relight_sl_off.log`. No production rendering file changed in this task. Full-scene OFF baseline diff remains unmeasured. |
| `zs_check.ps1 -Files <one file>` | **PASS**, `indra/newview/llfloaterdirector.cpp`, the sole touched C++ TU, in its own call; existing supported CachePath relocated into workspace. `.tmp/relight_sl_zs_director.log`. |
| Three generators | Regenerated **after the C++ edit**, then all three `--check` modes **PASS**. 37 authored pages, row pin 2158, five existing functional-manifest warnings. `.tmp/relight_sl_{kit,sources,manifest}{,_check}.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; `.tmp/relight_sl_prebuild.log`. Installed guard unchanged through the existing workspace-cache wrapper, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. ChecksOnly exited before object purge. |
| Scene/UI contract and JSON LF | **PASS**, source inspection verifies nine unique creative keys, shared save/load allowlist and sparse load, no quality/budget/debug keys; all five changed search destinations have real generated widgets. `.tmp/relight_sl_contract.log`. This is a source contract check, not an executed viewer scene round-trip. `git ls-files --eol` and direct byte checks confirm source/page-source/functional JSON are LF. |
| `git diff --check` | **PASS**; final run after this report, `.tmp/relight_sl_diff_check.log`. New report also checked directly because untracked files are not included by git diff. |

## 8. In-world checks still required

1. Save a scene with both masters on, nondefault source radius/falloff/floor/cap,
   distance/thickness/housing bias. Change all nine; Load must restore them.
   Quality preset, K, S, steps, half/full, denoise, Work budget and debug view
   must retain their current global values. Load an older scene and confirm the
   nine absent keys leave current values alone. Check autosave/recovery and undo
   through their normal UI flow. No shader reload should occur.
2. Search the new terms, reveal the real controls at minimum/default console
   widths, expand More options and read the wrapped help. Favorite ordinary
   Relight rows and adjust them from Quick; their real page controls must agree.
   Header master Quick support is still B7.
3. Fixed-camera full-viewer OFF-vs-baseline image diff and repeated identical
   ON frames (§6/§7), with scene, pose, exposure, FX, camera, resolution and
   settings pinned. Standalone shader fixtures do not replace these images.
4. Projector map-only/screen-only/hybrid A/B at 1080p: scaled/rotated housings,
   FOV sweeps, wall/floor/avatar contact, PCF/PCSS-lite/fill, native bias/offset,
   half/full and moving camera. Record both edges rather than assuming min()
   prevents displacement. Compare alongside sun and moon shadows.
5. Native shadow Preferences 0/1/2+, local light count 0/normal, no-shadow context
   flags, rig Off/Key/All, actor-only mask/F0/F3, bounce and mirror clones. Current
   native-vs-area suppression distinction is B2. Verify alpha hair stays on its
   supported shape-only path and nonrecipient surfaces never receive actor light.
6. Probe reflections, hero mirrors, Prism feeds, untiled/tiled snapshots,
   resize/resolution divisor and ReShade depth/normal sidecars. Check capture
   logs for effective workload clamps; measure actual GPU time/VRAM on the
   user's hardware before raising requested quality or enabling many lights.
