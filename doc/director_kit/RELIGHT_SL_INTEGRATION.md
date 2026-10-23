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

## 9. User decisions (10/21)

| Item | Decision |
| --- | --- |
| B1 | **Align to the SL lens.** Projector contact traces use the native optical origin and bias convention, so edges coincide with the SL map (1080p map+gather fixture, <=1 full-res px, no double edge; housed-lamp pool must not regress). |
| B2 | **Off means off.** An explicit Cast shadows opt-out and rig Off/Key/All also exclude the light from area shadows before top-K; the rig's bounce/catchlight follow the same setting. |
| B3 | **Add an opt-in mode** (default off): world light source radius from the emitting prim, safe clamps; rig fixture metadata wins. |
| B4 | **Independent + status.** No follower; the Soft shadows card shows a status line when Preferences shadows are off or reduced. |
| B5 | Keep as is. |
| B6 | Scenes only (as built). |
| Queued | Cinematic K-change flicker (P1), B7 searchable card-header switches + Quick, B9 repeatable takes, B8 VCam/Prism feed shadows. |

## Step 1 (10/21)

Implemented the three approved items on `feature/director-kit-p0`. The user
decisions above are preserved. No commit, push, stash operation, viewer build
or viewer launch; `.tmp/ref_private/` was not opened. Existing unrelated files
and the dirty `vcpkg` submodule are preserved.

| Item | Change / source anchors |
|---|---|
| P1: Cinematic K-change | `indra/newview/alareashadowmath.h:76` accepts the requested light cap separately from selected K. Admission reserves the frame ceiling for that stable cap; effective samples, steps, resolution and channels/draw stay fixed as visibility changes. Actual iteration accounting and dispatch still use selected K. `indra/newview/pipeline.cpp:26765` passes clamped `RenderAreaShadowLights`; `:26780` logs requested K alongside actual K. With requested K=8 at 1920x1080 Cinematic, K=7 and K=8 both use **S=2, steps=32, full resolution**. Actual work is 928,972,800 / 1,061,683,200 iterations; largest draw is 530,841,600, below 1500M/frame and 750M/draw. No hysteresis, timing or output history. |
| B2: Off means off | `indra/newview/pipeline.cpp:26675` excludes suppressed emitters before contribution ranking/top-K, leaving the next eligible UUID its channel. `indra/newview/alcinelightrigmanager.cpp:829` delegates ordinary lights to the existing emitting-prim/root-edit predicate (`pipeline.cpp:23310`); unflagged SL point lights remain eligible. `indra/newview/alcinelightrig.cpp:1889` applies Off/Key/All flags to both projector and bounce omni, including an omni whose projector is absent. Catchlights follow the key fixture's live flag for area selection. Their permanent native-map exclusion remains intact, so enabling a catchlight for area shadows does not enter it into the native slot auction. Mirror copies remain excluded. Declaration: `alcinelightrigmanager.h:1334`. |
| B4: Preferences status | `indra/newview/llfloaterdirector.cpp:3125` reads `RenderShadowDetail` during existing refresh and uses the kit conditional-row API: 0 shows “SL shadows are off in Preferences; soft shadows still on”; 1 shows “SL projector shadows are off in Preferences; soft shadows still on”; other values hide the complete row. It writes no setting and adds no listener. Real path: **Director > Light > Rim > Advanced > Soft shadows**, directly on the card before More options. Source: `scripts/code_tools/director_kit/director_mockup_data.json` (`area_shadow_sl_status`); generated widget: `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:232`. Row pin/comment: `scripts/code_tools/director_kit_gen.py:292`, 2159 rows. |
| Regression coverage | `indra/newview/tests/alrelightmath_test.cpp:271` checks stable per-light plans across visible K=1..requested, requested caps 1/4/8, all presets, 1080p/1440p/4K and budgets 1/100/1500, retaining hard frame/draw bounds. `scripts/code_tools/relight_area_policy_check.py:19` compiles the actual production predicate/rig/manager methods with minimal object stubs and exercises own/root flags, live transitions, next-channel replacement, ordinary points, Off/Key/All, bounce and catchlights. `scripts/code_tools/relight_area_check.py:64` uses its production 1080p Cinematic plan in the small GPU fixture and compares all seven persistent channels across K=7/8. |

The existing native map allocation, slot auction and collector count-before-filter
ordering are unchanged. No GLSL, shader define, shader-manager binding or setting
was added or changed. Both masters OFF retain the same shader sources and OFF
branches; catchlight native-map behavior remains unchanged. Full-viewer image
identity still requires the in-world baseline check below.

| Gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, exit 0: 31 production GLSL units/permutations, 126 rig rows with zero failures, runtime master/OFF probes, all area cases including the new CPU policy and K7/K8 GPU checks. `.tmp/relight_step1_shaders.log`. |
| `relight_area_check.py` | **PASS**, exit 0: new K7/K8 S2/steps32 plan and seven persistent RGBA channels byte-identical; suppression/replacement/live flag/child-root/rig cases PASS. Existing split draws, housing, independent blocker, all presets, deterministic gather/filter and moving-camera alignment cases remain green. `.tmp/relight_step1_area.log`. The 1080p planner is tested at its real dimensions; GPU readbacks use the existing small fixture, not a full viewer frame. |
| Tests | **PASS 15/15** `alrelightmath`; standalone test TU recompiled/relinked against existing runner/libraries, no viewer target. `.tmp/relight_step1_math_compile.log`, `relight_step1_math_link.log`, `relight_step1_math.log`. Existing nonfatal `vswhere.exe` environment diagnostic remains. |
| OFF/runtime preservation | **PASS** `relight_off_check.py`: eight disabled GLSL expression streams equal HEAD, live settings/no-reload/no-temporal checks pass. Production shader files are unchanged. `.tmp/relight_step1_off.log`. Full-scene OFF baseline diff remains unmeasured. |
| `zs_check.ps1 -Files <one file>` | **PASS**, separate calls for all five touched C++ TUs: `pipeline.cpp`, `alcinelightrig.cpp`, `alcinelightrigmanager.cpp`, `llfloaterdirector.cpp`, `tests/alrelightmath_test.cpp`; also cover the changed headers. `.tmp/relight_step1_zs_{pipeline,rig,manager,director,test}.log`. Existing supported CachePath relocated into workspace. |
| Three generators | Regenerated after C++ edits; `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py` and each `--check` **PASS**. 37 authored pages, row pin 2159, five existing manifest warnings. `.tmp/relight_step1_{kit,sources,manifest}{,_check}.log`. Generated hash changes on the other pages are expected. |
| XUI layout | **PASS** `xui_layout_check.py --changed HEAD`: all 40 regenerated XUI files, **0 findings**. `.tmp/relight_step1_xui.log`. |
| Source/JSON contract | **PASS** exact parsed-data comparison: one new read-only row and all unrelated source fields preserved; changed JSON files are LF. `.tmp/relight_step1_contract.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; `.tmp/relight_step1_prebuild.log`. Run through the existing workspace CachePath wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. ChecksOnly exited before object purge. |
| `git diff --check` | **PASS**, final run after this report; `.tmp/relight_step1_diff_check.log`. The new policy fixture is also checked directly for whitespace/conflict markers, since untracked files are not included by git diff. |

In-world checks still required (no viewer was built or launched):

1. At 1080p, requested lights 8/Cinematic/default budget, pan an eighth light
   into/out of the frustum while watching a persistent light's penumbra and
   visibility channel. Logs should retain S2/steps32/full; repeat the same
   settled camera/light set and compare images. Repeat at a lowered budget.
2. With more eligible lights than K, toggle Cast shadows on a selected light:
   it must lose its area channel immediately and the next ranked light takes
   it. Toggle back; verify deterministic ranking. Test a child light with the
   context flag on the child and separately on its root-edit prim. Unflagged
   ordinary point lights must still shadow visible opaque receivers.
3. Cycle rig Off/Key/All live with bounce fills and catchlight enabled: Off
   admits none, Key admits key and its bounce/catchlight, All admits all eligible
   fixtures. Verify catchlights still consume no native shadow-map slot and
   mirror clones stay excluded. Check ordinary and actor-only receivers.
4. Open the real Soft shadows card and change Preferences shadow quality
   0 -> 1 -> 2+ -> 0 without closing Director. Verify the two exact status texts
   and hidden row, including after card collapse/reopen/search, at minimum and
   default widths. No status code may change a creative/quality setting or add
   a reload; Preferences retains its existing native reload behavior.
5. Fixed-camera full-viewer OFF-vs-HEAD image diff with both Relight masters
   off, pinned scene/pose/exposure/FX/resolution, and repeated identical ON
   frames. Confirm native projector slots and lighting remain unchanged while
   OFF. The standalone fixtures above do not replace this full-frame evidence.


## Step 3 (10/21)

Implemented approved B3 as a runtime-only opt-in and corrected the Step 1
Preferences status text. No commit, push, stash operation, viewer build or
viewer launch. `.tmp/ref_private/` was not opened. Unrelated work and the dirty
`vcpkg` submodule remain untouched.

**Control:** Director > Light > Rim > Advanced > Light shape > **Size from prim**,
directly below World light size. `RenderLocalLightSizeFromPrim` is a persistent
Boolean, default **OFF**. Its one-sentence help is: "Use half the emitting prim's
smallest dimension, up to 1 m, while rig lights keep their own size."
Search keywords include "prim size", "light size" and "bulb size". The row has a
real generated `kit_switch`, setting binding, reset and page-watch entry.

**Bounding convention and precedence:** `ALRelight::primSourceRadius` in
`indra/newview/alrelightmath.h` returns `clamp(0.5 * min(scale.xyz), 0, 1)` metres.
The scale is the individual emitting `LLVOVolume::getScale()`, never its root,
linkset bounds, render/world AABB or influence radius. A 4 x 3 x 0.02 m panel
therefore gives 0.01 m; a large housing is capped at 1 m. Meshes use their prim
bounds rather than attempting to infer luminous faces or occupied geometry.
Nonfinite/nonpositive dimensions return zero. The existing shadow-only 0.10 m
disk fallback for a zero radius is unchanged; valid tiny positive bulbs retain
their actual smaller radius.

Projectors use the same smallest **XYZ** dimension, including thickness, rather
than a larger XY aperture. Cookies, optical origins and native map behavior
are unchanged. Attachments use their own authored prim scale in metres, with
no avatar/bone or linkset bound expansion. Child rotation does not enlarge the
inscribed sphere; changing the child's own scale takes effect live. Existing
world/attachment light admission switches still apply. These are conservative
bounding proxies, not claims that the whole housing is luminous.

`pipeline.cpp::area_source_radius` now supplies both the sphere-shading record
and gather disk radius, independently of the Light shape master. Fixture source
metadata wins before reading prim scale, including its existing reference
brightness distance and source radii above the world-only 1 m cap. Mirror rig
copies still use their own copied fixture metadata; manager/copy code is
unchanged. Catchlights/legacy rig metadata with the negative radius sentinel
retain World light size, preserving their existing global fallback.

The new creative setting is in the shared Director scene allowlist (A1), so
Save/Load and recovery include it. Existing sparse load behavior leaves it
untouched in old scenes. No quality/budget ownership, timing/history input,
shader define, shader listener, reload, uniform, native shadow allocation,
slot auction or count-before-filter ordering changed. The same bounded disk
sample/step workload runs; no extra render pass was added.

The Soft shadows Preferences status refresh now reads `RenderAreaShadows` too.
At native detail 0 it says "SL shadows are off in Preferences; Soft shadows is
off/on" according to the master; at detail 1 the prefix is "SL projector
shadows". The entire row remains hidden at detail 2+. The authored default
text says **Soft shadows is off**, matching the master default. No setting is
written and no listener was added.

| Required gate | Result / evidence |
|---|---|
| `relight_shader_check.py` + new sphere cases | **PASS**, exit 0: 31 production GLSL units/permutations, 44 links, existing 126 rig rows with zero failures; executed CPU-derived radii reach all eight production sphere paths (deferred point/batched point/projector and forward/alpha, legacy/PBR as supported), with explicit-radius equality, large-vs-tiny wrap positive controls and byte-identical master-OFF readbacks. RTX 5090 / NVIDIA 610.88 / GL 4.6. `.tmp/relight_step3_shaders.log`. |
| `relight_area_check.py` + new disk cases | **PASS**, exit 0: production gather consumes tiny/housing/panel/child/attachment/fixture/mirror/catchlight radii, exact explicit-radius readbacks and discriminating large-vs-tiny disk control. Shape is independent. Existing contact hardening, all presets, K7/K8 stability, housing/blocker, split-draw identity, determinism and moving-camera cases remain green. `.tmp/relight_step3_area.log`. |
| Math tests | **PASS 16/16** `alrelightmath`: added tiny bulb, large housing clamp, flat panel, scaled child, rotation-invariant dimensions, attachment, conservative projector thickness, zero/negative/NaN/infinite dimensions and the maximum boundary. Only the standalone test TU was compiled/relinked against existing test libraries/runner. `.tmp/relight_step3_math{_compile,_link,}.log`. Existing nonfatal `vswhere.exe` environment diagnostic remains. |
| Production CPU / OFF / status | **PASS** new `scripts/code_tools/relight_prim_size_check.py`: compiles actual radius/shape functions beside committed HEAD functions, checks OFF byte equality over global radii/physical states/ordinary and rig sources, and proves OFF never reads prim scale. ON checks live edits, shared sphere/disk radii and fixture/mirror/global-fallback precedence. Executes actual status block for native detail 0/1/2/3 x master OFF/ON. Also checks scene allowlist and real generated row/default/keywords. Included by both GPU gates; `.tmp/relight_step3_area.log`. |
| OFF shader/runtime guard | **PASS** `relight_off_check.py`: all eight disabled GLSL expression streams equal HEAD; all six shape settings, including the new opt-in, have no shader-manager/reload dependencies; existing current-frame checks pass. No production shader file changed. `.tmp/relight_step3_off.log`. |
| `zs_check.ps1 -Files <one file>` | **PASS**, separate calls for every touched C++ TU: `pipeline.cpp`, `llfloaterdirector.cpp`, `tests/alrelightmath_test.cpp`; also covers the changed math header. Supported CachePath relocated into workspace. `.tmp/relight_step3_zs_{pipeline,director,test}.log`. |
| Three generators and checks | **PASS** regenerated after final C++ edits, then `director_kit_gen.py --check`, `director_page_sources.py --check`, `director_functional_manifest.py --check`. Row pin/comment is **2160** (+1), 37 authored pages; five existing manifest warnings. `.tmp/relight_step3_{kit,sources,manifest}{,_check}.log`. Other generated panels change their source hashes only. |
| XUI / authored JSON / LF | **PASS**, `xui_layout_check.py --changed HEAD`: 40 touched XUI files, **0 findings**. Exact parsed source-data comparison confirms only the new toggle and status default changed; all changed JSON is LF. `.tmp/relight_step3_xui.log`, `.tmp/relight_step3_contract.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; `.tmp/relight_step3_prebuild.log`. Installed guard unchanged through the existing workspace CachePath wrapper; `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. Exited before object purge. |
| `git diff --check` | **PASS**, final run after this report; `.tmp/relight_step3_diff_check.log`. New test script also checked directly for whitespace because untracked files are outside git diff. |

The CPU fixtures exercise production functions with minimal object stubs; GPU
fixtures exercise production shader programs in controlled surfaces. They do
not substitute for an in-world scene round-trip or full-viewer image comparison.
After the separately authorized build, verify the real row at minimum/default
widths, scene save/change/load plus old-scene preservation, live prim scaling on
child/projector/attachment lights, rig/mirror sizes, and both Preferences status
master states. Full-frame OFF baseline equality remains unmeasured; standalone
CPU/GPU OFF bytes and unchanged production GLSL are the evidence recorded here.

## Step 4 (10/21), targeted fix round 4

Implemented approved **B7** across all kit pages. All **48 real card/option
header switches on 15 pages** are searchable. **14 of the previous 15 reviewed setting headers**
are Quick-favoritable, including **Soft shadows** and **Light shape**. The other
34 headers (23 controller-backed, seven code-bound, one tool-arming, three with
unproven/reloading listeners) remain
search destinations on their real pages. No commit, push, stash operation,
viewer build or launch; `.tmp/ref_private/` was not opened. Unrelated work and
the dirty `vcpkg` submodule are preserved.

`kit_pages.json` now explicitly classifies every **page/widget destination** in
`header_quick_policy`: `setting`, `controller`, `code`, `tool` or `reload`. Only `setting`
permits Quick, and it requires a setting binding with no hidden-page-dependent
commit behavior. Generation rejects missing/unknown classifications and a
setting-less safe declaration; a new page cannot inherit another page's policy.
Round 4 P0 fix: Quick eligibility is **default-deny**, independently of static
listener discovery. `header_quick_allowlist` in `kit_pages.json` contains the
14 reviewed setting names, one per header, each with a one-line justification.
A `setting` policy without allowlist membership fails generation; absence of
listeners never grants membership. Unused allowlist names, duplicate header
bindings and empty/multiline justifications also fail. New headers require human
review and an explicit allowlist addition before becoming Quick-safe.

`quick_safety_tripwire` supplements that authority by scanning sources across
`indra/newview`, including registrations outside `llviewercontrol.cpp`. It
recognizes `getSignal`, `getCommitSignal` and `getValidateSignal`, resolves
simple control/name and observer-lambda aliases, and vetoes unresolved controls,
setting names, signal aliases and callback symbols in reviewed registration
contexts. The central listener table is scanned in full (its existing external
RLV constexpr is resolved from `rlvdefines.h`). Literal ranges/prefix exclusions
and the callback reload trace remain. Setting associations with
`handleSetShaderChanged`, `setShaders`, `setGraphicsLevel` or
`handleRenderDeferredChanged` anywhere in newview veto Quick eligibility.
Resolved existing hint observers for Infrared/Soft edge and the VCam override
observer for Projector Cones are recorded in the setting justifications; the
scan is a supplementary tripwire, not a general C++ safety proof.

**14/15 remain Quick-safe; Depth of field is the only dropped header**
(`lens_group/focus_tab/dof_enable_check`, `RenderDepthOfField`). Its
`handleReleaseGLBufferChanged` listener calls pipeline GL-buffer teardown/creation
outside the local source; it has no safe-function approval. It joins the existing
`reload` exclusion policy (which now also covers unproven listener safety), remains
searchable on Lens > Focus, and keeps its native-page behavior. Soft shadows and
Light shape remain on the reviewed allowlist; their runtime paths add no reload dependency.
Rain occlusion and Cinematic lightning remain excluded.

Regression tests rebind a header to the real `DebugQualityPerformance` setting and
require rejection: with Preferences open its callback reaches `onChangeQuality`,
`setGraphicsLevel` in `llfeaturemanager.cpp`, then `setShaders`. Planted unknown
callbacks on helper and direct-signal registrations must also be rejected, including
a setting with multiple listeners. Round 4 adds allowlisted rejection fixtures for
getCommitSignal, getValidateSignal, aliased controls, nonliteral names, dynamic
direct listeners, signal/callback aliases and unknown callbacks, plus all four
cross-file reload APIs. These fixtures supply explicit allowlist approval so a
membership failure cannot mask a parser miss. Unused names, missing reasons,
shared header bindings and a new listener-free setting are also rejection cases.
Existing weather/direct/wrapped reload and lambda/connection/chained-setting
cases remain covered; no previous rejection expectation was relaxed.
The generated card/option `enable_quick` metadata is checked again by
`ALKitRow::createHeaderProxy`, including when rebuilding legacy saved favorites.
Path's Edit in world tool and every Props/Path header therefore cannot receive
hidden-page Quick commits: no stranded tool or stale sibling-control reread.
The card menu disables Add switch to Quick for those headers. Both menu enable
and click guards always permit removal of an existing favorite, including old
Path/Props or weather favorites. Additions and proxy creation still reject
unsafe/unknown policies; policy regression cases cover each category.

The generator derives search destinations from real `kit_card` / `kit_option`
metadata: title, option, tooltip, setting and synonyms. The **48-header pin** and
**2160 authored body-row pin** remain, with an updated round-4 comment. Existing
body/external search entries retain their content/order. Duplicate header/body
or header/option destinations fail generation. The functional manifest records
all 48 real headers and their Quick policies, independently checking exact
page/widget/card/setting agreement with the search index.

P2: restored the actual U+203A character in `Light › Rim` in `kit_pages.json`
and regenerated the Tron page and search index. Scanning every changed JSON/XUI
file also found older corrupted arrows, degree signs, superscript 2 and accented
preset names originating in `director_mockup_data.json`; corrected those source
strings and regenerated rather than patching generated bytes. Changed JSON stays
LF; the final JSON/XUI mojibake scan is clean.

`navigateTo` now reveals a header before row-setting fallback or the row reveal
guard, so an unrelated body row cannot block it. Reveal selects the real card's
mode/chip/option, scrolls to the switch and focuses its button. Changed only now
includes header-only state; setting headers compare against their saved default,
and code headers use their switch state. Global changed search checks the
individual header before any shared-setting body row, including selector options.
Header flips refresh the filter and page markers through `ALKitShell::runSearch`,
preserving the indexed widget/setting/synonym matches on external changes and
Undo. Row changed-filter refresh uses that same path; its narrower helper was
removed. The favorite action falls back to the active option's switch.

A safe Quick binding uses the source's native **toggle plus one commit**, with
no second setting binding/write. Generated safe header settings join the existing
single setting observer for undo. `widget_history=false` on the proxy avoids a
second widget entry; `ScopedSource` attributes the action to Quick while retaining
the source page's binding ownership. Live changes, defaults, saved identity,
removal and Show in Search use the real source; expired handles fail closed.
Long titles move below runtime header buttons when necessary; preset controls
get their own lower band. The layout selftest checks full title fit at
`CARD_MIN_W` with the real font and runtime controls.

`aluiperfselftest` adds `quick_header_proxy`: it builds the real Rim, Props and
Quick pages, toggles the Light shape Quick widget while its source is hidden,
asserts exactly one setting signal and source callback, rebuilds Quick and
checks retained state, rejects a legacy Spin favorite, and checks one-step
undo/redo with no duplicate history step. Final regressions enable Light shape,
search `relight` with Changed only, toggle OFF, then Undo and require the card
to reappear. They also check legacy unsafe removal eligibility and rejected
additions. The test restores the setting, search/filter state and favorites.
**Added and syntax-checked; not viewer-executed** under the no-build instruction.
`director_header_check.py` separately compiles production binding, lookup, reveal,
safety, changed-state, option-fallback and title-layout methods. The final
standalone regression also compiles production indexed query/scoring, shell
refresh/search and page search application, feeds the real generated Light
shape header fields, then checks enable / `relight` / OFF / Undo's setting
restore through the flip callback. It executes successfully without a viewer.
Its checkbox/
button model writes a real model setting, delivers change notifications and
executes registered callbacks with asserted model effects; it no longer replaces
callback execution with a commit counter.

Review item 9 is **not reproduced**: `ALKitPage::draw` calls `pollLiveValue` only
under the existing `mReadoutClock{10.f}` gate. Header availability refresh is thus
at most 10 Hz, not every frame; ordinary proxies call `enabledForEdit` on edit
operations, not from polling. No additional timing or cache behavior was added.

| Required gate | Final-round result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, exit 0: 31 production GLSL units/permutations, 44 links, 126 rig rows with zero failures, runtime/OFF and area probes. `.tmp/relight_step4_r4_shaders.log`. |
| `relight_area_check.py` | **PASS**, exit 0: CPU policy/prim-size, workload/K7-K8, split draws, housing/blocker, contact hardening, deterministic gather/filter and camera-motion cases. `.tmp/relight_step4_r4_area.log`. |
| New header cases | **PASS**, exit 0: production binding, lookup/reveal, live/default state, indexed `relight` Changed-only OFF/Undo restore, unsafe legacy removal/addition policy, option fallback, rebuild, minimum-width title/preset layout and expired source. `.tmp/relight_step4_r4_headers.log`. Viewer end-to-end assertions are added and syntax-checked but unexecuted. |
| Generator / manifest tests | **PASS**, exit 0: all 48 destinations, 14 allowlisted headers, planted commit/validate/control-alias/name/dynamic/signal/callback rejections, all four cross-file reload APIs, unused/unreviewed names, actual weather/direct/wrapped/cross-file quality and GL-buffer listener rejection, planted unresolved/multiple listeners, lambda/direct-connection/chained-setting parsing, dynamic range/prefix exclusions and unknown-name fail-closed and existing adversarial/rule fixtures. `.tmp/relight_step4_r4_{generator,manifest}_tests.log`. |
| Math tests | **PASS 16/16** `alrelightmath`, existing standalone executable; no viewer build. `.tmp/relight_step4_r4_math.log`. |
| OFF / runtime guard | **PASS** `relight_off_check.py`: eight disabled GLSL expression streams equal HEAD, no reload/temporal dependencies. `.tmp/relight_step4_r4_off.log`. |
| `zs_check.ps1 -Files <one file>` | **PASS**, separate calls for each touched TU: `alkitcard.cpp`, `alkitpage.cpp`, `alkitrow.cpp`, `alkitshell.cpp`, `aluiperfselftest.cpp`. Supported CachePath and TEMP relocated into workspace. `.tmp/relight_step4_r4_zs_{alkitcard,alkitpage,alkitrow,alkitshell,aluiperfselftest}.log`. |
| Three generators and checks | **PASS**, regenerated after the existing C++ edits and round-4 allowlist/tripwire and encoding changes, each followed by `--check`: kit, page sources and functional manifest. Five existing manifest warnings remain. `.tmp/relight_step4_r4_{kit,sources,manifest}{,_check}.log`. |
| XUI / LF | **PASS**: `xui_layout_check.py --changed HEAD`, **40 files, zero findings**. All 44 changed JSON/XUI files pass the UTF-8/mojibake scan; changed JSON is LF. `.tmp/relight_step4_r4_xui.log`, `.tmp/relight_step4_r4_contract.log`. Runtime title-fit assertion remains because serialized XUI cannot see header buttons. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; `.tmp/relight_step4_r4_prebuild.log`. ChecksOnly exited before object purge. Installed guard unchanged via the existing supported CachePath wrapper, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. |
| `git diff --check` | **PASS**, final run after this report update. `.tmp/relight_step4_r4_diff_check.log`. Untracked header test also checked directly for whitespace/conflict markers. |

No production rendering/shader/setting, GPU workload, native shadow allocation,
slot auction or collector count-before-filter code changed. Both masters OFF
retain identical rendering sources and paths. No new shader define/reload,
temporal or frame-history input was introduced. Full-viewer OFF image comparison
and execution of the new viewer assertions remain unmeasured; no viewer was built.

## Step 5 (10/21)

Implemented the safe, diagnostic portion of approved B9 on
`feature/director-kit-p0`. B1 remains parked on `wip/relight-b1-lens`; nothing
from that branch was read or merged. No commit, push, stash operation, viewer
build or viewer launch. `.tmp/ref_private/` was not opened. Existing unrelated
files and the dirty `vcpkg` submodule are preserved.

Relight's own area selector already orders by unmapped first, descending
contribution, then **object UUID** (`ALAreaShadow::before`). No pointer tie was
found in Relight's ranking, so its ordering is preserved and documented rather
than replaced. There is one candidate per emitting object, with no face ranking.
Test 17 instantiates the production selector with real `LLUUID` keys and
checks all **720 permutations** of six equal-score lights, both across the
four-channel admission boundary and with all six admitted, including mapped
ties. Fix round 1 gives the mapped lights LOWER UUIDs, so UUID sorting alone
fails and unmapped-first ordering is actually exercised. The native collector's
pointer tie remains untouched.

Successful Director ACTION firing (after its countdown), recorder Record and
recorder Play/resume now enqueue one-line LLSD notation for the dedicated
`<user logs dir>/relight_takes.log`. These are reached through the existing Director
transport and shared Takes/standalone recorder widgets. There is no existing
take-sidecar mechanism; saved camera-take files and their format are unchanged.
An ACTION suppresses its nested recorder hooks and queues exactly one record;
standalone recorder starts retain their own hooks. Each record has a diagnostic
`record_id`, which is never used as a render seed.
This ID is a local counter, avoiding any shared random-generator side effects.

The version-1 record contains:

- `settings_at_start`: all **18 Relight controls**, including both masters,
  source-size mode, falloff, trace/filter/debug controls and workload budget,
  plus native shadow detail, projector slot cap and local-light count.
- `area_view`: the **most recent allowed area pass**, explicitly labelled as
  such, with dimensions, outcome, candidate count and selected UUIDs in channel
  order (also score and sampled-map membership). Its sampled projector slot
  owners are stored alongside that pass, including vacant slots.
- The actual work plan when planning was reached: effective samples, steps,
  half/full, channels/draw, pixels, iterations, frame budget and draw cap.
  Exact 64-bit counts are decimal strings because LLSD integers are signed
  32-bit. Unobserved, disabled, unavailable, empty, rejected and failed states
  are explicit; a missing plan is not fabricated.
- `projector_slots_at_start`: current sampled slot owners and native fade
  values. These are separate from the area-pass owners so a changed assignment
  cannot silently be presented as belonging to that earlier pass.

The fixed-size diagnostic snapshot copies at most eight candidates and ten
slot UUIDs per allowed pass, only while Soft shadows is enabled. The reset and
slot-copy block runs after the existing OFF early return, adding no OFF frame
work. A disabled transport start constructs an explicit empty disabled area
record rather than presenting stale selection/work as current. Snapshot fields
are never read by selection, work admission,
render-target allocation, uniforms or shaders. There are no new settings,
shader defines, reload listeners, timing decisions or frame-history inputs to
rendering. Native allocation, the slot auction and count-before-filter ordering
are unchanged. The optional settled UI row is omitted: a fade-only observation
would not establish that the next collector/auction update will retain its set.
The documented generator row pin stays **2160**, with a fix-round comment; generated
XUI changes are source-hash updates only.

Test 18 exercises the production record serializer with a synthetic
1920x1080, three-light Medium selection: channels UUID 1/2/3, the third mapped,
518400 trace pixels, S8/steps12/half, **149299200** iterations, channels/draw 4,
1500000000 frame budget and 750000000 draw cap. It distinguishes earlier and
current slot owners, checks fades/vacancies, round-trips actual LLSD notation,
preserves a count above 32-bit, and checks absent/rejected plan diagnostics.
The synthetic record is printed in `.tmp/relight_step5_r1_math.log`.

Fix round 1 replaces the unreliable synchronous LLError record sink. Main-thread
snapshot construction copies scalar settings into an independent LLSD tree,
releases all main-thread subtree references, then transfers unique ownership.
`ALRelight::TakeWriter` holds at most **64 pending records**, evicts the oldest
when full, and reports the accumulated count as decimal `dropped_records` in
the next written record (zero otherwise). Failed appends also increment that
count. General workers serialize, append and flush each record; they never read
live pipeline/settings state or pass record data through LLError. No main-thread
viewer-log line is emitted (LLError flushes synchronously); see relight_takes.log.

A short queue mutex covers ownership transfers; a separate **blocking writer
mutex** covers drains and disk I/O, so contention cannot silently discard a
record and slow I/O cannot hold the enqueue mutex. Only one General drain is
scheduled at a time. A missing/closed/full General queue retains pending records
for the next start or shutdown. Cleanup closes and drains the writer before
worker teardown; it also works when the pool has already disappeared. Final
shutdown may wait for disk I/O. File-open/write failure cannot guarantee durable
output; its drop count is reported if a later record can be written.

New tests 19–21 verify the exact 64-record bound, newest-retained order and
single-report overflow accounting; a deliberately blocked sink with concurrent
drains, responsive enqueueing and real file append/flush/notation parsing;
escaped newlines, no lost or reordered records under contention; one outstanding
worker, post-failure retry, write-failure accounting, idempotent shutdown without
a worker and rejection after shutdown. These are standalone writer tests, not
an executed viewer transport session.

| Required gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, exit 0: 31 production GLSL units/permutations, production links, 126 rig rows with zero failures, live master/OFF and area probes. `.tmp/relight_step5_r1_shaders.log`. |
| `relight_area_check.py` | **PASS**, exit 0: CPU policy/prim-size, K7/K8, split draws, housing/blocker, contact hardening, byte-identical repeated gather/filter and camera-motion cases. `.tmp/relight_step5_r1_area.log`. |
| Tests and new cases | **PASS 21/21** `alrelightmath`; standalone test recompiled/relinked against existing runner/libraries, no viewer target. Strengthened real UUID permutations, serializer, queue bounds/drop accounting and slow-sink/contention/shutdown cases described above. `.tmp/relight_step5_r1_math{_compile,_link,}.log`. Existing nonfatal `vswhere.exe` diagnostic remains. |
| OFF / rendering preservation | **PASS** `relight_off_check.py`: eight disabled expression streams equal HEAD, no reload/temporal dependencies. Additional comparison reconstructs the complete HEAD pipeline token stream after removing only diagnostics, verifies the OFF early return precedes snapshot work, checks ACTION owns its one record and transport state/persistence matches HEAD. All shaders, shader-manager and control-listener files are byte-unchanged. `.tmp/relight_step5_r1_off.log`, `.tmp/relight_step5_r1_contract.log`. Full-viewer OFF image identity remains unmeasured. |
| `zs_check.ps1 -Files <one file>` | **PASS**, separate calls for all five touched C++ TUs: `pipeline.cpp`, `lldirectorcast.cpp`, `llflycamrecorder.cpp`, `llappviewer.cpp`, `tests/alrelightmath_test.cpp`; these cover both diagnostic headers too. Supported CachePath/TEMP relocated into workspace. `.tmp/relight_step5_r1_zs_{pipeline,lldirectorcast,llflycamrecorder,llappviewer,alrelightmath_test}.log`. Pipeline rechecked after moving the snapshot past the OFF return. |
| Three generators and checks | **PASS**, regenerated after final C++ edits, each followed by `--check`: kit, page sources and functional manifest. Row pin 2160 with fix-round comment; five existing manifest warnings remain. `.tmp/relight_step5_r1_director_{kit_gen,page_sources,functional_manifest}{,_check}.log`. |
| XUI / JSON LF | **PASS** `xui_layout_check.py --changed HEAD`: **40 files, zero findings**. Changed JSON remains LF. `.tmp/relight_step5_r1_xui.log`, `.tmp/relight_step5_r1_contract.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; `.tmp/relight_step5_r1_prebuild.log`. ChecksOnly exited before object purge. Installed guard unchanged via the existing supported CachePath wrapper, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. |
| `git diff --check` | **PASS**, final run after this report update. Both untracked diagnostic headers also checked directly for whitespace/conflict markers. `.tmp/relight_step5_r1_diff_check.log`. |

What remains non-reproducible: `mNearbyLights` membership still depends on native
frame-interval fades, its equal-distance ties still use drawable pointers, and
projector slot ownership/visibility still follows the native auction and fades.
Relight cannot stabilize a light excluded before its own ranking, or make an
identical hybrid image from different native map assignments/fades. Records
observe those assignments; they do not freeze, restore or replay them. The
area snapshot can predate a camera/settings change made by a transport start,
or survive a run of excluded views, which is why its source and current-start
fields are separate. This is start-event comparison evidence, not per-frame
capture metadata or a guarantee of repeatable full-shot pixels. Scene, camera,
pose, streaming, exposure and FX still need to be pinned. B1's lens alignment
and existing screen-space hidden/offscreen limits are unchanged. Full-viewer
OFF/ON repeated-frame captures and an in-world check of the actual transport
log events remain unexecuted, as no viewer was built or launched.

## Step 6 (10/21)

Fix round 1 addresses the agreed allocation-churn P0, prepass validity P1,
production-routing/ownership test P1 and repeated-feed-pass budget P2.

Implemented approved **B8 / D4** as a conservative, runtime-only opt-in on
`feature/director-kit-p0`. `RenderAreaShadowsVCam` is persistent and defaults
**OFF**. The real path is **Director > Light > Rim > Advanced > Soft shadows >
Also in VCam feeds**. This is one generated setting-backed switch, with search,
reset and watch bindings; its label wraps in a 48-pixel row to avoid elision.
The row pin/comment is **2161**. Two existing help texts explain the new opt-in.
The setting is global and is recorded in the existing take-start settings.

No commit, push, stash operation, viewer build or launch. B1 remains parked on
`wip/relight-b1-lens`; nothing was read or merged from it. `.tmp/ref_private/`
was not opened. Unrelated files and the dirty `vcpkg` submodule are preserved.

**View isolation.** Only a scoped, non-preview Prism camera render with its
exact-size scratch pack active may opt in. It executes the existing production
gather/filter pass after that feed's deferred geometry and before its local
lighting. Frustum/contribution selection uses the feed's `LLViewerCamera`;
light positions use its current modelview; deferred shader binding supplies
its projection, depth, normals and target extent. The two visibility planes,
gather and scratch targets belong to that auxiliary pack; none aliases the
main pack. A feed never reads the main visibility planes.

`beginPrismAuxiliaryState` / `endPrismAuxiliaryState` now save/restore area
validity, channel count, UUID-to-channel assignment, the last main take
diagnostic and its sampled slot IDs, only when both switches are enabled.
The existing scope still restores cameras, matrices, nearby lights, native
projector state and hardware-light records. Area-pass GL state uses its existing
complete guard. Entry clears validity/count immediately after saving, before
geometry or forward consumers can bind the main eye's channels. Scope exit
restores all saved area and take state without releasing targets. Feeds cannot
create, poll or advance the main area timestamp
queries, budget-change log state or summary timer. Early returns still close
the existing Prism scope.

**Deterministic selection.** Area ranking remains unmapped first, contribution,
then emitting-object UUID. The stateless feed collector additionally carries
that UUID as its equal-distance admission tie key in an allowed opt-in feed;
this removes its pointer-order boundary before the unchanged draw-count limit.
Every main-view and disabled-path entry carries a null key and retains the
original distance/pointer ordering. Native shadow-map allocation, projector
slot selection/auction code and count-before-filter ordering are unchanged.
No timing, retained visibility or prior quality plan enters sampling or quality
planning. Frame identity only resets the actual-pass budget counter below.

**One workload ceiling.** `ALAreaShadow::workload` retains the committed main
plan exactly. `feedWorkload` first reserves the iterations of that same plan
at the main target size and requested K (even when fewer lights are visible).
It divides only the remainder into **eight fixed equal allowances**, matching
`MAX_CAPTURES`. Each feed independently reduces samples, then steps, then
half-res, then skips. Each draw is also bounded by half its feed allowance.
`FeedPasses` admits each LIVE feed, by identity (its Prism registry slot), at most
once per frame (fix round 5, section 12; it was a bare pass count of eight):
a repeated render of the same Gate source, or a pass of a feed outside the frame's
live set, skips before allocation or gather, with invalid channels; a new frame
resets only this work counter.
Failed allocations conservatively consume an allowance. Main actual work + all feed work is at most
the reserved main work + eight allowances, which is at most
`RenderAreaShadowBudget`. This bound no longer assumes a scheduler cadence or
distinct feed IDs; unused allowances are deliberately not redistributed.
Missing main dimensions reject feed work. A full main reservation can leave
no feed shadows, without changing the main plan. `RelightQ` debug logging
reports the feed target, effective plan, iterations and allowance/skip.

**Target ownership.** Gather, scratch and visibility remain on the size-shared
Prism scratch pack and are reused across updates. Budget admission precedes
lazy allocation; each resized target allocates once. Feed planes survive a
temporary K reduction. Either master or feed opt-in OFF lazily releases the
area arrays over the entire pool, with no GL work on steady OFF updates.
`release_prism_scratch_entry` releases all three arrays on recycle/eviction;
`releasePrismLensBuffers` reaches that owner on screen-buffer release and
last-consumer close. A same-size Gate camera change reuses storage and reruns
selection/gather/filter with the incoming source. Partial allocation failures
release the failed pass's area arrays and publish no channels. The pool remains
bounded to eight size buckets, up to six RGBA8 targets each (24–96 MiB for a
2048x2048 bucket at half/full trace resolution). Resident VRAM and GPU timings
still need in-world measurement; the march ceiling does not bound filtering,
texture traffic or allocation latency on actual transitions.

**Deliberately omitted:** Light shape remains excluded in feeds. Its shared
forward/hardware-light upload/master state extends beyond this visibility
transaction, so enabling it was not a trivial safe extension. The area source
disk still uses existing source-radius metadata independently of that master.
Preview-atlas-only captures, probes/hero mirrors, HUDs, impostors, water
reflections and tiled snapshots stay excluded. The original main tiled-snapshot
flag is retained across a feed camera's zoom reset. Media and Gate composition
do not have a new camera/depth pass; Gate camera sources receive the opt-in
when actually rendered. Alpha stays on its existing no-area-visibility path.
B1's prim-centre projector trace origin, native map bias and screen-space
hidden/offscreen limitations remain unchanged.

| Required gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, exit 0: production compilation/44 links, 126 rig rows with zero failures, runtime OFF cases and all area/VCam cases. RTX 5090 / NVIDIA 610.88 / GL 4.6. `.tmp/relight_fix1_shaders.log`. |
| `relight_area_check.py` + new cases | **PASS**, exit 0: production dispatch and real GL ownership/state restoration, unchanged main sentinel, zero steady allocations/releases, one allocation per resized target, both planes/K changes, OFF/close/eviction, all six allocation failures/retry, Gate cuts, actual-pass cap and eight rejected mutations. Existing camera <=1px, housing, blocker, radii, contact-hardening and deterministic readbacks pass. `.tmp/relight_fix1_area.log`. |
| Production CPU fixture | **PASS** `relight_vcam_check.py`, included by both GPU gates: actual view/transaction/comparator/consumer methods, prepass invalidation and main diagnostic/channel restoration; no manually installed feed channels. UUID admission permutations, main/OFF pointer ordering, unsupported views, default-OFF UI, HEAD main-plan identity and shared budgets pass. `.tmp/relight_fix1_area.log`. |
| Tests | **PASS 22/22** `alrelightmath`, freshly compiled/linked against the existing standalone runner/libraries; no viewer target. Test 22 additionally checks repeated actual passes, frame reset, counter wrap and zero slots. `.tmp/relight_fix1_math{_compile,_link,}.log`. Existing nonfatal `vswhere.exe` diagnostic remains. |
| OFF/runtime preservation | **PASS** `relight_off_check.py`: eight disabled GLSL expression streams equal HEAD; runtime/no-reload/no-temporal checks pass. GLSL, shader-manager and reload listeners unchanged. `.tmp/relight_fix1_off.log`. Full-viewer baseline identity remains unmeasured. |
| `zs_check.ps1 -Files <one file>` | **PASS**, separate calls for both touched TUs: `pipeline.cpp` and `tests/alrelightmath_test.cpp`, covering both headers. Supported CachePath/TEMP inside the workspace. `.tmp/relight_fix1_zs_{pipeline,test}.log`. |
| Three generators and checks | **PASS** regenerated after C++ edits, then each `--check`: `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py`. Row-pin comment updated, 2161 retained; five existing manifest warnings. `.tmp/relight_fix1_{kit,sources,manifest}{,_check}.log`. |
| XUI / source contract / JSON LF | **PASS** `xui_layout_check.py --changed HEAD`: 40 touched XUI files, zero findings. All changed JSON files retain LF; no authored UI changes in this fix. `.tmp/relight_fix1_xui.log`, `.tmp/relight_fix1_json.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0; required `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`, existing workspace CachePath/TEMP wrapper. Installed guard unchanged; ChecksOnly exits before object purge. `.tmp/relight_fix1_prebuild.log`. |
| `git diff --check` | **PASS**, final run after report edits; untracked fixtures also checked for whitespace/conflict markers. `.tmp/relight_fix1_diff_check.log`. |

The new `relight_vcam_dispatch_check.py` compiles the production allocation,
gather/filter dispatch, channel consumer, view predicates, transaction, GL guard
and release-owner blocks with viewer wrappers backed by the gate's real WGL
context and production GLSL programs. The main visibility sentinel is produced
through that same dispatch and passed alongside feed packs, so routing to the
wrong owner is observable. Allocation counters require zero steady allocations
and releases (including K changes) and one allocation per resized target.
The production visibility-sampler binding block also runs after publication,
and real texture-unit queries verify both planes belong to the active pack.
All six partial-allocation failure positions, both OFF switches over inactive
buckets, close/screen teardown, eviction, Gate cuts and nine same-source passes
are exercised. Eight compiled mutations must fail: per-update release, missing
dispatch, main-pack dispatch or sampler routing, stale prepass validity, missing eviction cleanup,
uncapped passes and missing viewport restoration. The independent camera oracle
retains its one-pixel threshold; the old disconnected main-texture assertion
and manually installed feed channels were removed. These controlled fixtures
do not execute the whole viewer scheduler or actual close/Gate UI callbacks;
source checks verify those teardown routes reach the tested owner. No gate or
tolerance was weakened. Full-viewer image/state checks remain below.

In-world checks still required after the separately authorized review/build:

1. **Resize:** enable Soft shadows plus Also in VCam feeds, move a feed camera
   away from the main eye, and change its target size/aspect/resolution divisor
   repeatedly (including odd extents and large targets). Occlusion must follow
   that camera; no old-size stripes or main-view change. Watch allocation/VRAM
   peaks on transitions and steady reuse of the retained targets.
2. **Close and OFF:** close/delete the active feed, reopen it, then toggle the
   new setting and Soft shadows separately. Last-consumer close or OFF must
   release all pooled area targets; no stale
   channel, delayed main-view change or shader reload. Repeat with both Relight
   masters OFF and capture a fixed-scene full-viewer OFF-vs-HEAD image diff.
3. **Multiple feeds:** use 1, 2 and 4 feeds, including two that share a target
   size, with different cameras and light sets. Check each newly produced feed,
   channel ownership and byte-identical main visibility at a fixed main camera.
   Lower Work budget and raise requested K/Cinematic; feeds must degrade/skip
   while the main plan stays the same. Compare deterministic settled captures
   and `RelightQ` feed allowance logs; normal Prism cadence can retain older
   beauty outputs between scheduled updates.
4. **Gate switching:** cut and transition between camera sources with different
   sizes/light sets, and between camera/media/empty sources. Check the incoming
   source's own shadows, outgoing retained output and return to the main view;
   no cross-source channels or allocation growth after repeated cuts. Also
   exercise camera feeds used as projector cookies and face/monitor displays.
5. Repeat with native projector maps off/on, actor-only lights, rig suppression,
   ordinary points/projectors and tiled snapshots. Confirm main native slot
   owners/auction behavior and ReShade sidecars remain unchanged. Full-scene
   determinism still requires pinned scene/streaming/pose/exposure/FX and the
   native main collector/slot history described under Step 5; this change does
   not solve those existing full-viewer limits.

## Cleanup (10/21)

Implemented the ten review cleanup items on `feature/director-kit-p0`, against
`0ad7775b6d9`. B1 remains parked on `wip/relight-b1-lens`; nothing was merged
from it. No commit, push, stash operation, viewer build or launch.
`.tmp/ref_private/` was not opened; unrelated work and dirty `vcpkg` remain.

1. Camera feeds and surface lenses retain their last acquired scratch-size
   claim across normal scheduled updates and preview-only renders. Removal or a
   successful re-bucket releases the gather, scratch and visibility arrays of
   sizes with no remaining camera-payload consumer.
   Media/Gate registry entries cannot retain those area targets. Other scratch
   resources keep their existing lifetime. Same-size camera consumers retain
   shared area storage; OFF and full-pool eviction/last-close cleanup remain.
2. The GL dispatch fixture now executes the production `renderAreaShadows`
   entry and the actual local-lighting call expression, including OFF, view,
   light-count and program-readiness gates. Only scene collection is substituted
   by fixed fixture objects. Removing the real call or adding
   `|| sPrismLensRender` to the real early return fails the executable checks.
   The production guard is observed before subsequent consumer binding.
3. The prim-size OFF comparison is pinned to `abbf6ec0ae5~1`, before Step 3,
   rather than moving HEAD. Its radius/shape byte checks and no-scale-read
   assertions pass against that independent baseline.
4. Catchlights follow the live key's suppression flag when that projector
   exists. With an absent/dead key they fall back to the selected rig's shadow
   setting or the inactive rig's stored mode, so Off still suppresses them.
5. The retained suppression/clear helpers now visit each fixture's bounce omni
   as well as its projector, including an omni whose projector is absent.
   `applyShadowSuppression()` has no production callers, so its change is dead
   code and does not establish live focus suppression. The harmless omni clear
   remains; no callers were added.
6. Size from prim help explicitly says it overrides World light size for
   ordinary SL lights; fixture metadata keeps its existing precedence.
7. The Preferences status widget is cached when the lazy Rim page binds.
   Refresh uses the `LLHandle` when valid, falling back to a lazy `findChild`
   lookup and caching its result when the handle is null or expired.
8. The supplementary Quick tripwire traces helper definitions outside the
   listener's registration context. Direct-signal and setup-listener lambdas
   calling a helper chain that reaches `handleSetShaderChanged` are rejected.
   The reviewed default-deny allowlist remains the primary authority.
9. The perf selftest starts its flycam transport fixture with
   `startRecording(false)`, suppressing user take records.
10. The real Advanced section now orders **Light shape, Soft shadows,
    Diagnostics**. The kit source and generated outputs agree. All three
    generators were regenerated after the C++ edits and pass `--check`.
    The row pin stays **2161**, with a cleanup comment; changed JSON is LF.

**Fix round 1 (10/21):** the claim collector now uses occupied
`capture.cameraData()` entries, covering `SURFACE_LENS` as well as camera feeds.
Media/Gate slots do not retain claims. Preview-only renders skip claim updates
because they use separate scratch. The production lifecycle fixture exercises a
surface lens through repeated acquire/retain (one allocation, zero releases),
same-size camera sharing, removal in either order and last-owner release. It
also executes the real preview guard for both capture types. A compiled mutation
restoring the `isCameraFeed()` filter must fail those assertions. The production
status fixture covers a missing page, later widget lookup, cached refresh and
an expired handle followed by a replacement widget. Item 5 above corrects the
prior live-behavior claim; only its standalone helper body was exercised.

All results below were rerun after the round 1 C++ edits.

| Required gate | Result / evidence |
|---|---|
| `relight_shader_check.py` | **PASS**, production compile/link/runtime checks, 126 rig rows with zero failures, pinned prim OFF bytes, surface/preview/status cases and rejected claim-filter/dispatch mutations. `.tmp/relight_cleanup_r1_shaders.log`. |
| `relight_area_check.py`, including new cases | **PASS**, real GL unused-size pruning preserves the live bucket/main output, production entry gates, real-call and early-return mutations, surface/preview lifecycle (including rejected camera-feed-only mutation), lazy status/prim/policy fixtures, workload bounds, housing/blocker, contact hardening, camera alignment and determinism. `.tmp/relight_cleanup_r1_area.log`. |
| Tests | **PASS 22/22** existing standalone `alrelightmath` runner, no test C++ changed (`.tmp/relight_cleanup_r1_math.log`). Generator selftests, including both helper-wrapped rejection cases, and manifest rule/generation tests pass (`.tmp/relight_cleanup_r1_generator_tests.log`, `.tmp/relight_cleanup_r1_manifest_tests.log`). Production policy fixtures cover live/absent/dead key, selected/inactive rig fallback and the retained suppression/clear helper bodies (no production apply caller); lifecycle fixtures execute real removal/acquisition/re-bucketing with media survivors, surface lenses, previews and shared-size cameras. |
| `zs_check.ps1 -Files <one file>` | **PASS**, five separate calls: `alcinelightrigmanager.cpp`, `llfloaterdirector.cpp`, `aluiperfselftest.cpp`, `llprismlens.cpp`, `pipeline.cpp`. Each call passed; supported CachePath and TEMP are inside the workspace. `.tmp/relight_cleanup_r1_zs_{manager,director,selftest,prism,pipeline}.log`. |
| Three generated-output checks | **PASS** kit, page sources and functional manifest; five existing manifest warnings. `.tmp/relight_cleanup_r1_{kit,sources,manifest}{,_check}.log`. Other page changes are generated source hashes or source line references. |
| XUI / source contract / LF | **PASS**, 40 changed XUI files, zero findings (`.tmp/relight_cleanup_r1_xui.log`). Exact parsed source comparison finds only the three Advanced order indices and one help string changed; LF JSON and unchanged production GLSL verified (`.tmp/relight_cleanup_r1_contract.log`). UI draw lint also passes with zero new findings (`.tmp/relight_cleanup_r1_draw.log`). |
| OFF / runtime guard | **PASS**, eight disabled GLSL expression streams equal HEAD; no new reload/define or temporal dependencies (`.tmp/relight_cleanup_r1_off.log`). |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS all 11 gates**, exit 0, installed guard unchanged through the existing supported workspace-cache wrapper, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. Exits before object purge. An initial run without the established workspace Git configuration failed repository discovery; rerunning with that existing wrapper passed all gates. `.tmp/relight_cleanup_r1_prebuild.log`. |
| `git diff --check` | **PASS** after the final report edit; `.tmp/relight_cleanup_r1_diff_check.log`. The new lifecycle fixture also passes direct whitespace/conflict-marker checks. |

No production shader, sampling/history input, workload planner, native shadow-map
allocation, slot auction or count-before-filter ordering changed. These are
standalone CPU/GL checks; full-viewer OFF image identity, real-window behavior
and viewer GPU timing remain unmeasured under the no-build instruction.

## Step 2 (SL map + contact, 10/21), fix round 2

Implemented the user-approved third approach from HEAD `a63161d0028` on
`feature/director-kit-p0`. This supersedes B1's lens/bias alignment plan in
section 9. The two lens approaches remain parked; no code was merged from
either WIP branch. Their reviewed failures included half-resolution edge
displacement, lost penumbrae, receiver acne and erased thin contacts.

**Round 2 mechanism.** Ownership-only shortening dropped shadows at acquisition;
round 1's fade-interpolated ray length still moved discrete samples across
blockers. Both length-capping mechanisms have been removed. Mapped projectors
now march exactly the same full-length rays as points and unmapped projectors:
HEAD's distance limit, disk samples, step spacing, jitter, normal receiver bias,
source-local housing exclusion, crossing refinement and early return are kept.
Each validated hit supplies two visibilities from that one march:

- `full_vis`: HEAD's result (0 for a validated hit, otherwise 1).
- `contact_vis`: 0 only if that hit's refined `hit_t` is within the contact
  distance from the existing biased receiver ray origin; otherwise 1.

The gather accumulates each component over the same disk samples, then writes
`mix(full_vis, contact_vis, fade)` using the owning native slot's fade.
Fade zero is byte-identical to the unmapped/HEAD trace; fade one is contact-only.
In between, sample positions and both visibility components remain fixed as
fade changes. This makes coverage transfer continuous before RGBA8 quantization.
The blended visibility goes through the existing horizontal/vertical bilateral
filter and full-resolution upsample once. There are no separately filtered
components or inconsistent spatial weights. Projector composition stays
`min(native_map_visibility, screen_visibility)`, with native fade applied in
its existing location. Beauty shaders and the native map sampler are unchanged.

**Contact density tradeoff.** The contact zone uses HEAD's full-ray step density,
exactly like unmapped lights. It no longer gets a dense set of steps redistributed
inside a shortened 0.15 m ray. The 3/5 cm fixtures are missed at Medium's
12 steps, just as in HEAD; raising steps increases contact coverage within the existing workload planner.
The 3/5/10 cm positive controls use 64 full-ray steps for both current and
independently compiled HEAD shaders. This preserves a discriminating near
contact test without claiming the old shortened-ray density. The inherited
2 cm receiver-bias floor also remains.

**Setting and ownership.** `RenderAreaShadowMappedContact` is a persisted F32
setting, default **0.15 m**, read live with no define, reload or new listener.
Finite values are bounded to 0.001-64 m; nonfinite values use 0.15 m. Requested
zero/negative values disable settled contact detail (encoded as 0.001 m), while
the full trace continues to run and supplies fade-zero visibility.
`pipeline.cpp::renderAreaShadows` uploads four constant contact distances per
plane in `area_contact_dist` and four independent native fades in
`area_contact_fade`. Empty/unmapped channel fades are initialized to zero.
The existing `Candidate::shadowMap` resolves all ten current `mShadowSpotLight`
entries, including fading owners above a lowered auction cap. The owning slot's
`mSpotLightFade[slot]` is the same state that `setupSpotLight` consumes. Neither
is advanced between gather and lighting. VCam reads its swapped owners/fade one
and restores the main state; the CPU fixture executes the actual native
save/swap/restore assignments. Take logging includes the contact setting.

There is one trace invocation per disk sample, one unchanged step loop and the
same validated-hit early return. Classifying its hit adds no depth/normal read,
refinement, iteration or pass. The full-ray workload is identical to unmapped
lights; the planner, per-draw/frame ceiling, K reservation and shared main/VCam
budget are unchanged. No native allocation, auction or count-before-filter
ordering changed. There is no new timing/history input. Output is deterministic
given the current native owners/fades; inherited native fade timing remains a
capture reproducibility limitation. Both masters OFF still dispatch no gather.

**Independent 1080p proof.** `relight_contact_check.py` is invoked by both GPU
gates. It uses production gather, all three spatial filter/upsample passes and
the actual native PCF/index dispatch plus extracted projector composition.
Full and Medium half trace resolution are tested at **1920x1080**. This is a
controlled map/receiver fixture, not a full viewer map-render capture.

- Ten distinguishable native depth maps and matrices are checked against their
  own remapped slot-zero references; a compiled wrong-slot mutation must fail.
  CPU tests cover all slots, both planes, acquisition/eviction, retained zero
  owners, VCam and zero/nonfinite/live settings. Wrong-fade-slot and binary
  ownership mutations must fail; binary ownership also fails GPU acquisition.
- Quarter fades **0, 0.25, 0.5, 0.75, 1** and reverse exercise every slot. Dense
  **0.001** increments cover all **1,001** fade values, acquisition and eviction,
  with both a displaced native edge and an independently **lit native map**.
  The independent gather continuity bound is `ceil(255 * abs(delta_fade))`:
  at 0.001, at most **1/255**, before spatial rounding. The final RGBA8 image
  additionally accounts for four quantized writes (H, V, upsample and native
  hybrid), with a fixed `ceil(255 * abs(delta_fade)) + 4` bound. A trial 2/255
  final-image limit exposed a 3/255 half-resolution rounding transition. The
  test also replays such a pair's **same RGBA8 gathers through the same
  production filters and hybrid in FP32 targets**, enforcing <=1/255 plus
  floating point epsilon with intermediate quantization removed. This isolates
  spatial rounding while retaining the strict independent gather coverage bound;
  no expected visibility is derived from the current trace. Reverse final images
  must also match acquisition hashes.
- The previous fade-dependent **length interpolation remains a compiled
  mutation**. It must fail the same strict adjacent-gather or quantization-aware
  final-image continuity assertions with a lit native map at both resolutions. Production min-to-multiply mutation
  still fails a separate fractional-visibility oracle.
- At fade one, a 1 m blocker outside the 0.15 m contact zone adds no screen edge;
  each hybrid matches its own native map, with one boundary. The retained
  full-distance negative control must reproduce two separated boundaries at
  fade one too. Its displaced native edge leaves a lit gap before the screen
  shadow so the map cannot conceal the second edge.
- Points and unmapped projectors retain complete filtered HEAD byte identity;
  mapped projectors also match HEAD at fade zero. Near contacts, live housing
  transitions, all existing camera, deterministic readback, budget, K stability
  and VCam ownership regressions remain covered.

| Required gate | Round 2 result / evidence |
|---|---|
| `relight_shader_check.py` | PASS production compilation/linkage/runtime, rig regressions and the complete area/contact suite, including 1,001 fades in both directions at both resolutions, FP32 quantization replay and rejected mutations. `.tmp/relight_contact_r2_shaders.log`. |
| `relight_area_check.py` + new cases | PASS full/half 1080p: all 1,001 fades in both directions, strict gather <=1/255 per 0.001, final RGBA8 maxima **2/255 full / 3/255 half**. FP32 spatial/hybrid replay at the half-resolution rounding transition is **1.000015/255**, within the strict 1/255 + float epsilon bound. Old length interpolation fails the gather bound with **32/255** at both resolutions; all retained slot/binary/multiply/double-edge/HEAD and existing area/VCam regressions pass. `.tmp/relight_contact_r2_area.log`. |
| Tests | PASS **22/22** existing standalone `alrelightmath`; generator selftests and manifest rule/generation selftests. CPU contact fixture passes actual slot/fade packing, VCam restore, live settings and rejected mutations. `.tmp/relight_contact_r2_{math,generator_tests,manifest_tests}.log`, GPU logs. |
| OFF / runtime | PASS eight HEAD disabled expression streams, no reload dependency or temporal input. `.tmp/relight_contact_r2_off.log`. Full viewer OFF frame identity is unmeasured. |
| `zs_check.ps1 -Files <one file>` | PASS sole touched C++ TU `pipeline.cpp`, separate invocation with supported workspace CachePath/TEMP. `.tmp/relight_contact_r2_zs_pipeline.log`. |
| Three generators / `--check` | PASS regenerated after C++ edits and each checked. Row pin **2161** retained with a round 2 comment; five existing manifest warnings. `.tmp/relight_contact_r2_{kit,sources,manifest}{,_check}.log`. |
| XUI / JSON LF | PASS **40 touched XUI files, zero findings**; unchanged real widget trees and LF JSON verified. `.tmp/relight_contact_r2_{xui,contract}.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all **11 gates**, required safecrlf environment and supported workspace cache/TEMP wrapper; exits before object purge. `.tmp/relight_contact_r2_prebuild_final.log`. |
| `git diff --check` | PASS after the final report edit; the new untracked contact fixture is also checked directly for whitespace/conflict markers. `.tmp/relight_contact_r2_diff_check.log`, contract log. |

No commit, push, stash operation, viewer build or launch was performed.
`.tmp/ref_private/` was not opened; unrelated files and dirty `vcpkg` remain.
Full-viewer identity, in-world quality and viewer GPU timing remain unmeasured
under the no-build instruction.

## Env lights regression (10/21)

### Diagnosis and disposition

Compared working build `c6c0ae0af63` with running build `e0cc289b108` and
inspected HEAD `c69d8e94fd6`. HEAD has no runtime/script difference from the
running build. **No certain cause was established; no runtime or tracked test
change was made.** The diagnostic fixture and logs are under `.tmp/relight_env*`.
The reported mapped-contact override of 8 m and shadowed-light cap of 8 are
accepted as negative results, not proposed again as fixes.

One newly effective exclusion is reproducible: step 1 makes an emitter's own
or root-edit object's session **Cast shadows off** flag exclude its area-shadow
channel. With an empty flag set, both an ordinary non-rig world point and a
non-rig world projector receive channels in both builds. There is no evidence
that the affected in-world objects actually have those flags. Removing the
filter would undo the intended Cast shadows policy, so that is not a justified
fix on the available evidence.

### Production path and eliminated mechanisms

| Stage / suspect | Evidence |
|---|---|
| Selection, `pipeline.cpp:26789` onward | `toggles()` admits ordinary world lights through `bdmerge_should_render_light(false,false)`; attachments have independent toggles, and synthetic rig emitters bypass the world toggle. `actually_drawn()` intersects with `mNearbyLights`, retaining the native count-before-colour/range/frustum limit. These main-view policies are unchanged across the range. |
| Actor-only bits | World lights with zero actor bits use the world collector branch. An unrelated actor-only rig does not give world objects actor bits. The diagnostic executes the actual selector with actor-mask programs ready and unavailable; both world emitters retain channels. |
| New suppression, `pipeline.cpp:26832`, `alcinelightrigmanager.cpp:829`, `pipeline.cpp:23359` | Ordinary objects fall through to `isProjectorShadowSuppressed()`: exact own UUID, then exact root-edit UUID. A flagged linked-build root can therefore suppress all its emitting children. There is no region-wide wildcard or region-ID insertion in this predicate. Rig policy writers use registered projector/omni/catchlight IDs. `applyShadowSuppression()` has no active call site; its retained helper only touches registered rig IDs. An unrelated root flag does not suppress the fixture's world lights. |
| Distance / priority / requested K | Relight still ranks unmapped first, descending contribution, then emitting-object UUID. Main `calcNearbyLights()` ordering and its native membership/fade behavior are unchanged. Requested K now controls work degradation, not the draw-collector limit and not which admitted UUID is published. It can reduce sampling; see the separate hypothesis below. |
| Channel publication and lookup | Selection stores `volume->getID()`; gather resolves that same UUID against `mLights`, stores it in `mAreaShadowIDs`, uploads its centre/radius and UUID seed, then publishes count/valid after filtering. `deferredLightShape()` compares the same emitting volume's own UUID, including children. It assigns `.w` independently of source radius, so a zero-radius ordinary point still gets a channel and the gather's 10 cm fallback. No root/source UUID substitution was found. |
| Point dispatch | Outside the light box, the volume point draw uploads `LIGHT_SHAPE` from `deferredLightShape(volume)` (`pipeline.cpp:27930`). Inside it, `light_shapes` is appended in parallel with accepted fullscreen lights, copied to `shapes[count]`, and uploaded with the matching light batch (`pipeline.cpp:27959`, `28039`, `28077`). There is no world/rig distinction in this packing. |
| Projector dispatch | Both volume and fullscreen projector draws call `setupSpotLight()`, whose `LIGHT_SHAPE` upload uses the same volume (`pipeline.cpp:29313`). Native slot/fade lookup uses the drawable, independently of the area channel. |
| Shader consumers / uniforms | The shared bind hook uploads Relight bit 1 from `areaShadowsActive()` on normal and fast binds. The active render-target pack supplies `area_shadow_map0/1`. Point, multi-point and spot shaders read shape `.w`, select plane by channel < 4, component by channel & 3, and apply visibility. These consumer files did not change in the suspect range. |
| Step 5 snapshot / allowed hoist | Allowed-view evaluation still precedes selection and dispatch; the new take snapshot is written after OFF/view gates. Snapshot data is not read by selection, admission or shading. No main-view change to the allowed predicate was found. |
| Step 6 auxiliary lifetime | With both masters enabled, begin saves valid/count/IDs, clears validity/count, and end restores all three plus diagnostic state. The fixture executes these actual blocks and the real lookup after restoring different auxiliary IDs. With feeds disabled, auxiliary invalidation is harmless to this ordering: `llviewerdisplay.cpp:944` runs Prism before main rendering; main deferred lighting resets validity at entry and rebuilds channels at `pipeline.cpp:27758`, immediately before light draws. No auxiliary/mirror/probe invocation was found between that gather and its opaque light consumers. A global invalidation there would also remove rig area channels. |
| Step 2 mapped contact | Ordinary points and unmapped projectors keep full visibility tracing; contact/fade packing is zero for them. The existing GPU fixture compares their filtered bytes with pre-step-2 `a63161d0028`; its shader sources are unchanged from `c6c0ae0af63`. Native-mapped projectors alone receive contact distance and slot fade. The user's contact-cap result is retained as a ruled-out explanation. |

### Diagnostic fixture and results

Run `python .tmp/relight_env_diag.py` from the repo root. This compiles only
standalone fixtures, not the viewer. It extracts the actual collection block
from **each** build, compiles production suppression/lookup methods and current
auxiliary save/restore blocks with small object wrappers, and uses the actual
publication statements. Camera/frustum, nearby membership and object properties
are supplied inputs; it does not recreate a live simulator's native collector.
The old-build comparison substitutes the old selector, not an entire old viewer.

Fixture emitters: world point UUID 10, child world projector UUID 20 under root
100, synthetic rig emitter UUID 30 and attached emitter UUID 40, equal scores,
shuffled `mLights`, and native draw limit 256. Results in
`.tmp/relight_env_selection.log`:

- Both builds: point channel **0**, world child projector channel **1**, rig
  channel 2 and attachment channel 3. Giving the projector a sampled native
  shadow slot moves it to channel **3**, and its lookup still matches.
- Both world channels survive actual auxiliary save/clear/restore, unrelated
  root flags, actor-only readiness changes and K=4/K=8 work admission.
- Today's selector alone excludes the world point when UUID 10 is flagged,
  or the child projector when root UUID 100 is flagged. Removing the flag
  restores eligibility immediately. The old selector ignores those flags.
- Both selectors exclude later world entries when the native collector limit
  is 2, including when the first two entries are dark. Raising that limit to 4
  admits both world lights. This is the unchanged native count-before-filter
  behavior, distinct from `RenderAreaShadowLights`.
- Turning the world-light class off excludes both world objects while leaving
  the rig and attachment eligible in both builds. This also removes their
  illumination; it is relevant only if the lights themselves are missing.

### Ranked remaining hypotheses

1. **Own/root session shadow suppression.** This is the only reproduced new
   selection exclusion matching world objects losing channels while unrelated
   rig/attachment lights retain them. A root flag covers a linked build's child
   emitters; it is not evidence of a region-wide flag. A fresh login clears the
   session set, so failure on newly rezzed unlinked lights after login would
   strongly downgrade this explanation.
2. **Requested-K degradation with the actual Custom settings / scene geometry.**
   `RenderAreaShadowQuality=4` means **Custom**, not Cinematic. Its callback does
   not apply samples, steps or resolution. These actual settings and framebuffer
   dimensions were not supplied. The planner comparison proves that channels
   survive while sampling may decrease: at 1920x1080 full-resolution Cinematic,
   one visible light goes from 11x32 to 5x32 for requested K=4; at 3840x2160 it
   goes from 2x32 to 1x32. K=8 at 3840x2160 becomes 1x22. Medium defaults at
   1080p remain 8x12 half-resolution. This can affect thin/distant blockers, but
   the fixture does not reproduce the reported total world-light failure.
3. **World-light toggle or native nearby membership/draw limit.** These can
   distinguish world lights from rigs/attachments, but no change to their main
   policy was found. The toggle is unlikely if world illumination remains.
   The native `RenderLocalLightCount` is a separate limit from shadowed top K.
4. **Unobserved auxiliary/render-path lifetime issue.** Downranked by call order,
   successful production transaction/GL ownership tests and continuing avatar
   area shadows. No specific failing main-view sequence was found. A full
   viewer capture could still reveal a sequence absent from these fixtures.

### Exact in-world checks

Record original values before temporary changes and restore them afterward.

1. Compare the existing failure with a **fresh login**, all VCam/Prism camera
   feeds closed and `RenderAreaShadowsVCam=false`. Fresh login clears session
   Cast shadows flags. Check one newly rezzed **unlinked** point light, then one
   newly rezzed projector. A recovery confined to this test points to session
   flags or auxiliary state; a failure on fresh objects rules out inherited
   linked-root flags for those objects.
2. For an affected linked projector, select the **root** normally and inspect
   right-click **Cast Shadows**; check it if unchecked. Also inspect the emitting
   child through Edit linked / the Practicals or Lightbox selected-light control.
   The UI checks the selected object's own flag, while rendering also checks its
   root, so a checked child alone does not clear a suppressed root. Compare the
   same light before/after, then compare a fresh unlinked copy. No repeat of the
   mapped-contact or top-K=8 experiment is needed.
3. Use opaque matte floor and blocker prims, both visible in the main camera,
   with a single nearby world emitter and the rig disabled through its UI.
   Example: 10 m floor, 0.4 x 0.4 x 0.8 m blocker resting on it, light 1 m to the
   side and 1.5 m above the floor, light radius 5 m. Keep the light/blocker/floor
   in view and compare `RenderAreaShadows` OFF/ON. Test point and downward-facing
   white-cookie projector separately. This avoids judging only transparent
   hair/clothing, which do not consume these opaque screen-space channels.
4. Verify `BDMergeLightToggles=false` temporarily, or
   `BDMergeRenderWorldLights=true` if keeping that master on. Confirm the world
   emitter still illuminates the floor. Temporarily use High quality (preset 2:
   12 samples, 20 steps, half resolution), `RenderAreaShadowBudget=1500`, and
   `RenderLocalLightCount=256`. This tests the unknown Custom plan and native
   collector limit without assuming Quality=4 means a particular plan.
5. At the settled failing view, briefly start/stop the existing Takes recorder
   to enqueue a snapshot into the viewer log directory's `relight_takes.log`.
   Copy the emitting prim's key (and root key for linked lights) and compare it
   with `area_view.selected[].uuid/channel`. Record `area_view.status`, candidate
   count, effective work, dimensions, `settings_at_start`, sampled map ownership
   and fades. This snapshot describes the **most recent allowed area pass**.
   If the world UUID is absent, investigate admission/suppression; if present,
   set `RenderAreaShadowDebugView` to **channel + 1**, then return it to 0.
   White visibility with an admitted UUID directs investigation to ray coverage;
   a visible shadow in this debug view but absent in lighting directs it to
   lookup/uniform/consumer state. Move the camera inside then outside the point
   light's 1.5 x radius box while retaining the same fixture in view to compare
   fullscreen multi-point with the volume point path. Finally reopen feeds one
   at a time with feed shadows enabled and repeat the UUID/debug check.

### Validation and limits

- Production selection comparison and planner matrix: **PASS**;
  `.tmp/relight_env_diag.py`, `.tmp/relight_env_selection.log`.
- `relight_area_check.py`: **PASS**, including its production CPU policy,
  prim-size, VCam lifecycle/view, dispatch/GL ownership and contact sub-harnesses;
  `.tmp/relight_env_area.log`. Real GPU coverage includes channels/UUID
  permutations, point/unmapped filtered byte identity, camera movement, main/feed
  ownership, rejected mutations and all 1,001 fades in both directions at full
  and half resolution. The respective final continuity maxima are 2/255 and
  3/255; FP32 quantization replay is 1.000015/255 within the existing epsilon.
- `relight_shader_check.py --shader-ref HEAD`: **PASS**, 31 production GLSL
  units/permutations, 44 program links, live/OFF readbacks and 126 surface/rig
  matrix rows with zero failures; `.tmp/relight_env_shader_surface.log`. HEAD's
  runtime/shader sources equal the running build. This mode omits the separate
  area suite. An earlier duplicate invocation reached that suite and was
  interrupted; its partial log is `.tmp/relight_env_shader.log` and is not
  counted as a completed gate.
- OFF gate: **PASS** all eight disabled expression streams and live-setting
  checks; `.tmp/relight_env_off.log`. No runtime code was changed; full-viewer
  OFF framebuffer identity was not measured.
- Existing standalone `alrelightmath` executable: **22/22 PASS**;
  `.tmp/relight_env_math.log`. Generator and manifest selftests pass;
  `.tmp/relight_env_generator_tests.log`, `.tmp/relight_env_manifest_tests.log`.
- All three generator `--check` modes pass: kit and page sources through the
  guard, functional manifest separately (`.tmp/relight_env_manifest_check.log`).
- `prebuild_guard.ps1 -ChecksOnly`: **all 11 gates PASS**, using the existing
  workspace CachePath/TEMP wrapper with `GIT_CONFIG_COUNT=1`,
  `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`;
  `.tmp/relight_env_prebuild.log`. Compile gate reports no changed runtime TUs
  since `e0cc289b108`; there is no touched C++ TU requiring a separate Zs check.
- `git diff --check`: **PASS** after the findings appendix; final evidence in
  `.tmp/relight_env_diff_check.log`.

No commit, push, stash, viewer build or viewer launch was performed.
`.tmp/ref_private/` was not opened. Existing unrelated files and dirty submodules
are preserved. Live scene membership, session flags and the user's actual GPU
frame remain unobserved; the passing fixtures do not prove an in-world fix.

### Round 2: same world emitter, PROJECTOR versus POINT

**Disposition:** no certain cause of the complete reported failure was found.
No runtime or tracked test was changed. The new diagnostic fixtures are
`.tmp/relight_env_r2_diag.py` and `.tmp/relight_env_r2_bind.py`; their logs use
the same basename. This round accepts the user's negative results for contact
8 m and shadowed lights 8. It does not propose repeating those changes.

The additional facts materially change the earlier ranking: the same light
still illuminates and gains soft shadows when switched to POINT. Own/root
shadow suppression applies equally to both classifications, so it is no
longer the leading explanation. The known sampling-budget/noise topic is
separate. `CineLightRigShadowMode=1` controls registered rig fixtures, not
ordinary world projectors. Quality 4 is Custom; it does not specify a sample
or step count.

#### End-to-end source comparison

Compared `c6c0ae0af63` with `e0cc289b108`. HEAD `c69d8e94fd6` has no runtime or
script difference from `e0cc289b108`.

1. **Admission:** `renderAreaShadows()` still starts with `mLights`, requires
   LIGHT state, non-HUD, enabled class, positive range/colour and camera-frustum
   intersection, and intersects with the native `mNearbyLights` draw budget.
   `toggles()` and `actually_drawn()` have no projector-only exclusion. Their
   count-before-colour/range/frustum behavior is unchanged. Step 1 added
   `registry.areaShadowSuppressed(volume)` alongside mirror exclusion.
   `projectorRenderParams()` recognizes a real enabled spotlight immediately,
   before checking Prism enablement or bindings; selection and the world draw
   use that same classification and projector toggle. No new classification
   rejection was found.
2. **Native ownership and channel assignment:** a projector is mapped only
   when its exact drawable equals one of all ten current `mShadowSpotLight`
   owners. Projector classification alone does not mark it mapped. Pending
   targets are not owners. Selection ranks unmapped before mapped, then score,
   then emitting UUID, as yesterday. A mapped projector can consequently lose
   top-K admission to unmapped lights. This priority is not new in step 2.
   Gather publication and `deferredLightShape()` use the emitting prim's own
   UUID, including children; `.w` is assigned independently of source radius.
3. **Gather in step 2:** every sample still executes the full ray. `areaTrace`
   returns `.x = full_vis` and `.y = contact_vis` from its first validated hit.
   The gather accumulates each and writes
   `mix(full_vis, contact_vis, area_contact_fade[channel])`. The one blended
   component is filtered H/V and upsampled; those operations do not select
   `.x` or `.y` again. The contact uniform is `area_contact_dist`, packed from
   `RenderAreaShadowMappedContact` only for a mapped candidate. Its fade comes
   from that drawable's current owning `mSpotLightFade[slot]`, not channel
   number, pending target, rig mode, or the complement uploaded to lighting.
   Both contact and fade start at zero for unmapped/empty channels.
4. **Contact range:** finite contact settings clamp to 0.001-64 m; nonfinite
   uses 0.15 m. The independent `area_params.x` trace limit clamps to 0.1-64 m.
   Ray length is `min(max(distance(target,origin)-end_gap,0), area_params.x)`.
   `end_gap = max(source_radius,0.1) + area_light_bias`. The contact test is
   `contact_distance > 0.001 && hit_t <= contact_distance`. `hit_t` is measured
   along the disk-sample ray from the **biased receiver origin**, not from the
   light, and not simply camera-depth separation. There is no hidden 0.15 m
   clamp and no contact-dependent ray shortening or step redistribution.
   A contact cap of 8 exactly recovers the full component whenever the trace
   limit is at most 8 (including MaxDist 8). For MaxDist above 8, only hits
   within 8 along the receiver ray are guaranteed equivalent. Thus 8 is not
   universally identical to a possible 64 m full trace.
5. **Projector consumer:** `spotLightF.glsl` is unchanged across the two builds.
   It first computes native visibility
   `clamp(sampleSpotShadow(...) + (1-native_fade), 0, 1)` for a valid slot.
   It then computes `min(native_visibility, filtered_screen_visibility)` when
   bit 1 of `relight_enabled` is set and `light_shape.w >= 0`. The consumed
   screen component is the filtered gather blend, not unconditionally contact.
   With no slot native visibility starts at 1, so the hybrid retains the full
   screen shadow. `min` cannot brighten away an existing screen shadow; a hard
   native edge can nevertheless dominate the appearance.
6. **World draws and binding:** both volume and fullscreen projector loops
   call `bindDeferredShader()` and `setupSpotLight()`. The latter uploads
   `LIGHT_SHAPE = deferredLightShape(volume)` while either shape or area is
   active, and resets native index/fade to -1/1 for an unmapped draw. The common
   deferred binder binds `AREA_SHADOW_MAP0/1` for spot programs too, selecting
   the second plane only when count exceeds four. `bindGlobals()` reaches
   `bindAnimated()` even with Rim mode zero; it refreshes the runtime master
   as `(shape ? 1 : 0) | (area_active ? 2 : 0)`. Full production volume and
   fullscreen programs, both auxiliary and fallback permutations, link in both
   builds with live locations for master, shape, both area samplers, native
   slot/fade and Rim mode. No missing spot permutation or bind was found.

#### Step 1 suppression writers

`sNoShadowProjectors` is a session UUID set, cleared on relog. The mutation
entry point inserts/removes exactly its argument; there is no world, region or
root wildcard. `isProjectorShadowSuppressed()` checks emitter UUID, then
`getRootEdit()` UUID, without testing point versus projector.

- Rig Off/Key/All updates only its owned projector/omni emitters. Key mode
  suppresses non-key fixtures. Mirror/catchlight writers likewise use their
  created emitter IDs; area catchlight policy follows the registered key.
- Focus suppression's `applyShadowSuppression()` remains a retained helper
  with no active call site. Its body and clearing helper address registered
  rig emitter IDs, not object-target or focus-target UUIDs.
- A world projector **or its root can enter the set** through the explicit
  Cast Shadows UI. Context-menu iteration flags selected roots; Practicals and
  Lightbox flag the primary selected object. A flagged root therefore affects
  child emitters. The UI's checked state only tests the selected object's own
  flag. This exclusion is real but, for unchanged emitter/root IDs and flags,
  also suppresses POINT. The fixture executes and asserts that equivalence.

#### Requested fixture and observations

The CPU comparison extracts and compiles each revision's real selector plus
production suppression, publication and lookup methods with supplied object,
camera and nearby wrappers. It covers a world child projector, the same UUID
switched to point, with/without an owner, and own/root flags. Both builds admit
unflagged world point and projector channels. Current suppression rejects both
classifications equally. A crowded case with eight unmapped competitors and
K=8 excludes the owned world projector, then admits that same UUID at channel
zero when switched to point. This also happens in both builds. The independent
binding fixture executes production
area binding, spot slot/fade upload, shape upload and runtime-master statements
for K=1/4/5/8, both planes, every channel, all ten slots, no slot, and fades 0/1.

The GPU fixture runs production gather, filters and extracted native-map/hybrid
composition at 1920x1080, full and half trace resolution, using yesterday's
independently compiled committed shaders as the baseline. The scene has a
visible foreground blocker plane 1 m from receiver Z=-4, a world light at
(1,0,-2), zero configured source radius (the production 10 cm fallback disk),
8 samples, 12 steps and MaxDist 8. The native map is independently all-lit so
it cannot hide a lost screen shadow. This is a controlled receiver/map fixture,
not a captured viewer scene or an actual native map render.

| World projector | Native fade | Contact 0.15 | Contact 8 |
|---|---:|---|---|
| No native slot | 0 | Soft shadow; baseline bytes | Soft shadow; baseline bytes |
| No native slot | 1 (irrelevant; packed as 0) | Soft shadow; baseline bytes | Soft shadow; baseline bytes |
| Native slot 9 | 0 | Soft shadow; baseline bytes | Soft shadow; baseline bytes |
| Native slot 9 | 1 | No screen shadow for this distant hit, expected | Soft shadow; baseline bytes |

All 16 combinations pass, including deterministic replay and disabled hybrid
ignoring stale area inputs. CPU packing separately covers all ten native slots,
both planes, live caps, main/VCam save/swap/restore and rejected wrong-fade-slot
and binary-ownership mutations. The reported large-cap failure is **not**
reproduced. There is no failing production regression case supporting a fix.

#### Remaining hypotheses, ranked against the complete new facts

1. **Mapped demotion outside the actual selected top K.** The same UUID becomes
   an unmapped-priority candidate as POINT, while an owned projector ranks
   after all unmapped candidates. The crowded production-selector fixture
   reproduces this at cap 8 in both builds. Even cap 8 cannot include it if
   eight higher ranked candidates remain. This needs a different live
   candidate/ownership set to explain yesterday versus today; it is not
   evidence for changing selection policy. The user's actual selected UUID
   list is not available in this round.
2. **Unobserved projector consumer state in the live render sequence.** A lost
   `.w`/area master or overwritten visibility binding could preserve ordinary
   illumination and point shadows and ignore contact changes. No such sequence
   or changed statement was found; extracted binding tests and both revisions'
   full linked programs pass. This ranks by fit to the observations, not by a
   reproduced defect. Full-viewer interleaving remains outside these fixtures.
3. **Contact-only policy in the effective failing pass.** This is the strongest
   known projector-specific behavior change at the default 0.15 cap: a settled
   mapped projector no longer receives distant screen shadows. It cannot
   explain a consumed cap 8 with validated hit distances <=8. Only a different
   effective pass setting or longer receiver-ray hits with MaxDist >8 would
   keep that explanation viable. The user's unsuccessful override is accepted,
   and this report does not establish either condition.

Own/root suppression is substantially downranked by the same-emitter switch;
world-toggle rejection conflicts with continuing illumination. Custom sampling
and the known K-budget noise change do not by themselves explain a projector
losing the same admitted full/large-cap visibility while POINT retains it.

#### Round 2 validation

- New selector/channel, spot binding and 16-case GPU matrix: PASS;
  `.tmp/relight_env_r2_{diag,bind}.log`.
- OFF gate: PASS all eight disabled shader expression streams and live-setting
  checks; `.tmp/relight_env_r2_off.log`. No runtime changed, so this round adds
  no OFF-path difference. Full viewer framebuffer identity remains unmeasured.
- Existing standalone math runner: PASS 22/22;
  `.tmp/relight_env_r2_math.log`. Generator selftests and manifest rule/generation
  selftests pass; `.tmp/relight_env_r2_{generator_tests,manifest_tests}.log`.
- All three generators `--check`: PASS without regeneration;
  `.tmp/relight_env_r2_{kit,sources,manifest}_check.log` (five existing manifest
  warnings).
- Installed `prebuild_guard.ps1 -ChecksOnly`: PASS all 11 gates, required
  `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`,
  `GIT_CONFIG_VALUE_0=false`, and the existing workspace CachePath/TEMP wrapper;
  `.tmp/relight_env_r2_prebuild.log`. The compile gate reports no changed C++
  TUs since the running build. No touched `.cpp` needs a separate `zs_check`.
- All ten `relight_*_check.py` gates: PASS. The full shader driver executes area,
  policy, prim-size, rig, contact, VCam, dispatch and lifecycle gates; OFF runs
  separately. `.tmp/relight_env_r2_shaders.log` exits 0, including 1,001 fades
  at 0.001 increments in both directions at full and half resolution. Strict
  gather maximum is 1/255; final maxima are 2/255 full and 3/255 half. The half
  FP32 replay is 1.000015/255, within float epsilon of the strict 1/255 bound.
  Wrong-slot/fade, binary ownership, length interpolation, multiply composition
  and existing dispatch/lifecycle mutations are rejected.
- Final `git diff --check`: PASS; `.tmp/relight_env_r2_diff_check.log`.

No commit, push, stash, viewer build or viewer launch was performed. No read of
`.tmp/ref_private/` occurred. Prior report edits, unrelated files and dirty
`vcpkg` are preserved.


## Thin occluders (10/21)

**Fix round 7 (10/22):** the Thin objects switch could still strip VCam feeds of
their area shadows once `RenderAreaShadowBudget` was lowered (about 930M and
below at 1080p/K8; for example Low, 250M, 1920x1080 main, K8, 1280x720 feed: ON
left 1.18M per feed against 14.66M with OFF, so even the uniform fallback failed
and the feed published no shadows). OFF is unchanged (HEAD RGBA8 identity, HEAD
planner identity).

1. **Feed guarantee replaces the flat 1/32 holdback** (`kFeedHeadroomDivisor` is
   gone). The OFF feed allowance is `(budget - OFF main reservation) / 8`. A feed
   draws at all (uniform plan: 1 ray, 1 step, half resolution) once its
   allowance reaches `pixels(half) * max(requested K, 2)`; for a 2048x2048 feed,
   the largest camera capacity, that is `1,048,576 * max(K,2)`. A segmented
   main plan now plans against `budget - feedReservation()` with
   `feedReservation = 8 * min(OFF allowance, that need)`. The OFF allowance is
   bounded by the cheapest OFF main plan (1 ray, 1 step, half resolution), which
   keeps the reservation and therefore segmented admission **independent of the
   preset**. A segmented plan that does not fit inside it is the uniform (OFF)
   main plan, deterministically, and the feeds then get exactly the OFF
   allowance. So with Thin ON every feed gets **at least
   `min(its Thin OFF allowance, the 2048x2048 minimum)`** and draws wherever the
   OFF feed draws. Above that minimum the main view may spend the rest, so a
   feed can have fewer rays (and fewer channels per draw) than OFF; it never has
   none. The reservation is at most 67.1M (K8) or 16.8M (K1).
   Example, Low, 1080p main, K8, 1280x720 feed: **250M** OFF 14.66M, ON 14.66M
   (the main view falls back to the uniform plan); **1500M** OFF 170.9M, ON
   21.6M (segmented main keeps its 1327M; the feed still draws, 4 channels).
   Test 25 sweeps budgets 100/300/600/1000/1500 x main 1080p/1440p/4K x K1..8 x
   all presets x feeds 512x288, 640x360, 854x480, 720p, 1080p and 2048x2048,
   asserting feed channels > 0 wherever OFF > 0, ON allowance >= min(OFF
   allowance, minimum), one shared budget, and exact OFF plans after a main
   fallback. The raw per-draw channel count is *not* asserted `>=` OFF (it is
   anti-monotonic in the allowance: a smaller allowance trims rays and so puts
   more lights in one draw); the test prints how often ON has fewer.
2. **Common far floor.** All presets thin the far segment, as a last resort and
   one step at a time, to `min(authored count, 8)` (`kThinFarFloor`, Low's
   uniform far count): never coarser than OFF Low. High and Cinematic no longer
   fail the segmented march while Low keeps it (their floors were 20 and 32).
   Segmented admission is therefore identical for every preset and the far step
   count is common to all of them. Test 24 now sweeps 100..1500M (and 720p-4K,
   K1..8): every preset draws, segmented-ness is common, far steps are common
   and samples, rays per pixel and resolution do not decrease with the preset.
   Uniform-fallback rows are the OFF plan; its preset inversion at tight budgets
   (for example 4K/K8/1000M uniform: Medium 5x12 vs High 3x20) predates this
   switch and is out of scope here. Test 14 was updated for the new floor
   (the "Cinematic far floor is its uniform 32" expectation described the old rule).
3. **Budget log.** A Thin request that fell back to the uniform march is now a
   clamp (`thinDropped=1`), and the tracked plan includes `thin` and the
   effective `nearSteps`, so toggling the switch or crossing the fallback logs
   once.
4. **Take record.** Feed passes write their own `mRelightFeedAreaTake`. The main
   view's record (and projector slots) is no longer written by feeds at all;
   `area_view` stays the main view. The most recent feed pass is a separate
   `feed_view` (`source` `most_recent_vcam_feed_area_pass`, `view` `vcam_feed`),
   present only while Soft shadows and the VCam feed switch are on.

**Fix round 6 (10/22):** ten second-round review findings, all in the ON path;
OFF is unchanged (independent HEAD RGBA8 identity, planner identity).

1. **Feeds keep their shadows.** The segmented main plan filled about the whole
   1500M budget and never drops steps, so feeds were left 0.9-22M and several
   got no channel. The main plan now holds back a feed reservation (round 7
   replaced the flat 1/32 with `feedReservation()`) and a feed whose segmented
   plan does not fit falls back to the authored **uniform** march and plan
   (`feedWorkload(q, uniform, ...)`). The fallback is a plan property: the
   shader mode uniform follows `work.q.nearSteps > 0`, so a fallback feed also
   uses the OFF start offset. Test 23 asserts feed channels > 0 for 512x288,
   640x360, 854x480, 720p and 1080p feeds with 1..K lights, main 1080p and 4K,
   cap K1/K4/K8, every preset, plus the one-budget total.
2. **Monotonic presets.** Segmented plans keep full resolution only while 12
   rays (High's request) fit, so Cinematic is never left with fewer rays than
   High (it was 4 full-res vs 12 half-res at 1080p K1/K2, 1600x900 K<=3, 720p
   K<=5). Test 24 sweeps 720p/900p/1080p/1440p/4K x K1..8 x all presets at
   1500/1400/1200M: effective samples, rays per pixel (full = 4 half) and
   resolution never decrease with the preset.
3. **Help.** "Use fewer Shadowed lights or a lower resolution" (preset does not
   change grain). Settings comment, kit row, index, mockup data and the three
   generated outputs agree.
4. **Near spacing.** The near span is **1.85 m**; 46 regular samples are spaced
   evenly over [2 cm floor, 1.84 m), 3.96 cm apart (a 4 cm rail cannot fall
   between them; round 5 was 4.26 cm), then the two join samples at 1.84/1.86 m
   are the last two near steps, so no interval is charged twice and the old
   6.9 cm hole and the [2 cm, 4.9 cm) gap after the first sample are gone. The
   straddle is skipped for fewer than four near steps.
5. **Short rays.** The floor is `min(0.02, near_dist)` and every sample is
   clamped to `limit`.
6. **Fail-closed cliff.** Under budget pressure the far segment thins one step
   at a time **down to the common floor** (round 7; round 6 used the authored
   uniform count, which left Cinematic with a floor of 32)
   after rays and resolution are spent and before the view fails; if the
   segmented plan still cannot fit (for example Cinematic, whose floor is 32,
   or a very small budget) the main view and feeds use the uniform plan. ON
   therefore draws wherever OFF draws. Test 14 asserts this on a grid.
7. **`relight_thin_check.py` entry point.** Running it directly now launches
   the GPU harness with `--area-only --thin-only` (it used to import and exit 0).
8. **Table assertions.** The resolution, far and sample tables are explicit
   per-row literals for OFF and ON (the `thin || preset != 3` expression could
   not fail).
9. **Take record.** `far_steps` is clamped at zero; the record also carries
   `view` (`main` or `vcam_feed`) and `uniform_fallback`.
10. **Authored steps.** Each pass records the authored count it read itself
    (`authored_steps`; feeds share the one global setting) and its view label.

**Fix round 5 (10/22), user decision:** thin-object tracing is now opt-in.
`RenderAreaShadowThinObjects` is a persisted Boolean, default **false**.
At the default 1500M budget, the 48+32 march leaves only 1-2 effective rays
at 4K, so its finer blocker coverage can make soft edges grainier. This
supersedes the unconditional march and preset changes from the earlier rounds.

**OFF** retains HEAD `d4f4ec1192ff` behavior: preset requests are Low
4/8/half, Medium 8/12/half, High 12/20/half, Cinematic 24/32/full;
uniform intervals use the original `0.25+0.5*jitter` start offset. The
planner still reduces samples, then uniform steps, then resolution. The
quality listener is identical to HEAD. Toggling Thin objects never rewrites
saved quality values. With both lighting masters off, the existing disabled
lighting expression/readback checks continue to cover the unchanged paths.

**ON** uses 48 near steps through min(1.85 m,limit): 46 regular samples evenly
over [2 cm, join - 1 cm) and the final two straddling the join at -/+1 cm
(round 6; spacing 3.96 cm), then at least 32 far steps
for every preset. Near sample distance is floored at **2 cm**, in addition
to the receiver normal bias (never beyond the ray: `min(0.02, near_dist)`). Custom retains its saved far count; presets
retain authored counts above 32. Runtime admission upgrades preset far density without
changing the saved counts. The deterministic pixel-cell/light-UUID phase is
`.15 + .8 * ((37*i mod N) + u) / N`; the [.15,.95) phase is
retained with the separate 2 cm minimum sample distance. No timing, frame history, channel or feed-slot input enters sampling.
Housing exclusion, receiver bias, light-end gap, refinement, contact
classification and native-fade blending retain their existing operations.
The mode and near/total counts are runtime uniforms: no shader define,
reload listener or new shader-manager dependency.

The planner charges `pixels * requested K * samples * (near+far)` and bounds
every split draw. ON chooses half resolution unless 12 full-resolution rays fit,
then reduces rays from the original request, then resolution, then thins only
the far segment toward the common floor, 8 (near steps are never
reduced). A plan that still cannot fit uses the uniform plan, so ON never draws
less than OFF. The segmented main plan plans against the budget minus the feed
reservation (at most 67.1M); feed allowances still reserve main work first. Native shadow allocation, slot auction and
count-before-filter ordering are unchanged. These are iteration ceilings,
not measured GPU time or bounds on refinement/filter reads.

The real UI destination is **Director > Light > Rim > Advanced > Soft shadows
> Thin objects**, enabled with Soft shadows. Help is: "Catch railings, fences and poles; uses more steps, so soft edges
get grainier. Use fewer Shadowed lights or a lower resolution." Budget defaults
to its maximum 1500M; budget help describes the reduction order for both modes
and the uniform fallback. Search terms are thin, railing, fence, pole, detail. The source
row binds the setting directly; generated index/page/functional outputs expose
the real widget. Row pin **2162** has a comment. The setting joins the shared
Director scene allowlist, so Save/Load/recovery use the normal sparse `has()`
behavior. Quality and budget remain global. Take records include
`RenderAreaShadowThinObjects` in `settings_at_start`. Each planned snapshot
also records `authored_steps`, effective `near_steps`, `far_steps` and total
`steps`. Authored counts describe the sampled pass even if settings change
before take start. The Steps settings comment documents far-count semantics.

Production-planner tables, default **1500M/frame, 750M/draw**; ON plans against
the frame minus the feed reservation (1433M at K8, 1466M at K4). H/F means
half/full trace resolution. "Draw M" is the largest actual channel batch.
The standalone math test asserts literal tables and prints both modes.
Tests 15/22 cover K7/K8 and feed reservations with Thin objects OFF/ON;
test 12 includes an authored preset far count of 53.

**OFF - uniform march**

| Output | K | Preset | Effective samples | Steps | Trace | Frame M | Draw M |
|---|---:|---|---:|---:|---|---:|---:|
| 1080p | 4 | Low | 4 | 8 | H | 66.4 | 66.4 |
| 1080p | 4 | Medium | 8 | 12 | H | 199.1 | 199.1 |
| 1080p | 4 | High | 12 | 20 | H | 497.7 | 497.7 |
| 1080p | 4 | Cinematic | 5 | 32 | F | 1327.1 | 663.6 |
| 1080p | 8 | Low | 4 | 8 | H | 132.7 | 66.4 |
| 1080p | 8 | Medium | 8 | 12 | H | 398.1 | 199.1 |
| 1080p | 8 | High | 12 | 20 | H | 995.3 | 497.7 |
| 1080p | 8 | Cinematic | 2 | 32 | F | 1061.7 | 530.8 |
| 4K | 4 | Low | 4 | 8 | H | 265.4 | 265.4 |
| 4K | 4 | Medium | 8 | 12 | H | 796.3 | 597.2 |
| 4K | 4 | High | 9 | 20 | H | 1493.0 | 746.5 |
| 4K | 4 | Cinematic | 1 | 32 | F | 1061.7 | 530.8 |
| 4K | 8 | Low | 4 | 8 | H | 530.8 | 265.4 |
| 4K | 8 | Medium | 7 | 12 | H | 1393.5 | 696.7 |
| 4K | 8 | High | 4 | 20 | H | 1327.1 | 663.6 |
| 4K | 8 | Cinematic | 1 | 22 | F | 1459.8 | 729.9 |

**ON - segmented march**

| Output | K | Preset | Effective samples | Near + far steps | Trace | Frame M | Draw M |
|---|---:|---|---:|---|---|---:|---:|
| 1080p | 4 | Low | 4 | 48 + 32 | H | 663.6 | 663.6 |
| 1080p | 4 | Medium | 8 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 4 | High | 8 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 4 | Cinematic | 8 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 8 | Low | 4 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 8 | Medium | 4 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 8 | High | 4 | 48 + 32 | H | 1327.1 | 663.6 |
| 1080p | 8 | Cinematic | 4 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 4 | Low | 2 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 4 | Medium | 2 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 4 | High | 2 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 4 | Cinematic | 2 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 8 | Low | 1 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 8 | Medium | 1 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 8 | High | 1 | 48 + 32 | H | 1327.1 | 663.6 |
| 4K | 8 | Cinematic | 1 | 48 + 32 | H | 1327.1 | 663.6 |

**Keep and include `scripts/code_tools/relight_thin_check.py` in the eventual
commit.** Both shader and area gates import it. The new OFF case independently
links HEAD's gather and trace utility, then compares complete RGBA8 bytes
against the current executable with the uniform false. It covers every preset,
full and half resolution, three UUID seeds, unmapped visibility and mapped
contact fades 0/.25/.5/.75/1, with positive blocked-pixel controls. Values come
from the production planner. The C++ VCam fixture independently compiles HEAD's
planner and compares OFF quality, steps, resolution, pixels, channels and total
iterations across both resolutions, budgets, dimensions, requested K and
selected counts. ON feed reservations remain bounded by the shared budget.

The prior round's independent oracles run explicitly with the switch **ON**:

- Dense railing: 4 cm rail, fixed 32-ray/64-uniform-step reference with 2 m
  cap; darkness ratio **[0.8,1.2]**, every preset, three seeds. Known-lit pixels
  must be 255 and repeated readbacks byte-identical. HEAD must fail at least
  three presets. The reference is fixed independently of the tested preset.
- Far panel: 1920x1080 depth, 64 seeds including 1152/12348, every preset at
  full/half; final may increase HEAD-blocked pixels by at most **2/255**.
  Independent-phase and quadratic-spacing mutations must fail that same sweep.
- Join: 4 cm rail centered exactly 1.85 m (the join) from the biased receiver,
  every preset, full/half, 64 seeds; every tested receiver must shadow. Dropping
  only the join pair must fail the same fixture.
- Near sweep (round 6): a 4 cm rail every 5 mm from 9 cm to 1.85 m along a ray
  with x component 0.3, Low and Cinematic, full/half, two seeds; every disk ray
  must be blocked (353 positions x 2 x 2 x 2). The round-5 layout (2 m span,
  4.26 cm spacing, pinned pair replacing two samples) must fail the same sweep
  (30 rail/preset/seed/pixel cases). Static checks cover the floor and the
  `limit` clamp, which a pixel cannot observe.
- Straddle guard (round 6): 0..5 near steps, full/half: a wide wall 1.9 m along
  the ray stays fully shadowed and a clear ray fully lit.
- Curved receiver: analytic convex cylinder, every preset/full/half/three
  seeds; both N.L > .2 and **0 < N.L <= .2** remain **255/255**. The grazing
  band uses a point-like aperture so every tested ray is unoccluded; a finite
  disk can physically straddle the terminator.
- Existing housing, contact/fade continuity, split/K identity, camera/VCam,
  ownership, restoration and mutation regressions retain their tolerances.

Round 6 gate records are in `.tmp/thin_r6/` (`final_*.log`, `zs_*.log`,
`*_check.log`, `prebuild.log`); each gate ran once after the final edit and
regeneration. Findings that drove the changes: the sweep and the join fixtures
showed the round-5 layout leaves a 6.9 cm hole before the join (the round-5
mutation fails 30 sweep cases); the join fixture moved from 2 m to the new
1.85 m join with its zero-visibility assertion unchanged. OFF arithmetic still
executes only in its own branch; independent HEAD RGBA8 identity passes. The
dispatch fixture stub gained the `feed` / `segmentedRequested` fields. The two
full GPU suites retain their 1,001-fade acquisition/eviction checks. Separate Zs
checks use the installed script, one TU per process, with the cache inside the
workspace. Standalone test compilation retains the existing nonfatal
`vswhere.exe` diagnostic. Concurrent runs share fixture directories under
`.tmp/relight_step*`; run the GPU gates one at a time.

| Gate / acceptance | Final result | Evidence |
|---|---|---|
| `relight_shader_check.py`, including OFF bytes and all ON regressions | **PASS** | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` and `--thin-only`, same production-GPU oracles | **PASS** | `final_area.log`, `final_area_thin.log` |
| `relight_thin_check.py` run directly (new entry point re-launches the harness) | **PASS** | `final_thin_direct.log` |
| `relight_off_check.py`: disabled expression identity, no reload/temporal dependencies | **PASS** | `final_off.log` |
| Standalone `alrelightmath` tests: literal tables, feed admission (23), monotonic grid (24), fallback, take record | **PASS 24/24** | `final_math*.log` |
| Generator selftest; functional rule/generation selftests | **PASS** | `gen_selftest.log`, `manifest_selftest.log` |
| Separate `zs_check.ps1 -Files <one file>`: pipeline, llfloaterdirector, llviewercontrol, test | **PASS** (4 TUs) | `zs_*.log` |
| Regenerated three kit outputs; each `--check` | **PASS** | `kit.log`, `sources.log`, `manifest.log`, `*_check.log` |
| `xui_layout_check.py --changed HEAD` | **PASS, 0 findings / 40 files** | `layout.log` |
| `prebuild_guard.ps1 -ChecksOnly`, required safecrlf environment | **PASS, all gates** | `prebuild.log` |
| `git diff --check` and untracked thin-fixture whitespace | **PASS** | `diff_check.log` |

No viewer build/launch, commit, push or stash operation. No private-reference
access. Synthetic fixtures do not establish whole-viewer image identity or
continuous intersection of hidden/offscreen geometry; in-world validation
and measured GPU cost remain outside this no-build task.

**Round 7 gate records** are in `.tmp/thin_r7/` (each gate ran once after the final
edit; the GPU gates ran one at a time):

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py`; `--area-only --thin-only` | **PASS**; **PASS** | `shaders_full.log`, `shader_thin.log` |
| `relight_area_check.py` (includes the VCam CPU/dispatch fixtures and the HEAD planner identity) | **PASS** | `area.log` |
| `relight_thin_check.py` run directly; `relight_off_check.py` | **PASS**; **PASS** | `thin_direct.log`, `off.log` |
| Standalone `alrelightmath` tests | **PASS 25/25** (new: 25 feed admission grid; 14, 18, 23, 24 updated) | `math.log` |
| `zs_check.ps1` per touched TU: `pipeline.cpp`, `alrelightmath_test.cpp` | **PASS** (2 TUs) | `zs_pipeline.log`, `zs_alrelightmath_test.log` |
| Kit generator `--check` x3 (kit gen, page sources, functional manifest) | **PASS** | `director_*_check.log` |
| `prebuild_guard.ps1 -ChecksOnly` (safecrlf environment) | **PASS, all gates** | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

The dispatch fixture stub gained `mRelightFeedAreaTake`. No viewer build/launch,
commit, push or stash; no private-reference access.

## Importance ranking + stack review fixes (10/22)

User decision: rank every shadowed light **by on-screen importance, regardless of whether it
has a native SL map**. The Opus + GLM full-stack review fixes and the small items from the
Thin objects reviews ride in the same round, followed by **fix round 1** (section 4) for the Codex,
GLM and Opus reviews of that change, **round 2** (section 5), and the final **round 3** (section 7: per-light
housing reverted, hand-over margin, small fixes). Start: HEAD `84a3f731d1c`. No commit, push, stash,
viewer build or viewer launch; `.tmp/ref_private/` was not opened; everything below is runtime-only
and deterministic (no history, hysteresis or timing in any ranking, sampling or allocation input).

### 1. Ranking formula (`NV/alareashadowmath.h`, `ALAreaShadow::importance`)

```
importance = luminance * range / max(distance, range / 2) * (1 + 0.25 * (1 - nativeWeight))
nativeWeight = slot fade (0..1) * native-map validity at the light's own view depth (0..1); 0 when unmapped
```

* `luminance`: Rec.709 luminance of the linear colour x `AlchemyGlobalLightScale` x the emitter's
  Local lights EV scale (this frame's value, exactly what the old score used).
* `range`: the influence range (`getLightRadius() * 1.5`, metres); `distance`: camera to light centre.
* The camera term saturates at `range / 2`, so it is **never zero because the camera is outside the
  range** (the old `ALRelight::legacy(distance, range, falloff)` was 0 beyond the range, so practicals
  5-10 m away tied at 0 and were ranked by UUID alone, and dollying across one lamp's boundary popped
  another lamp's shadow). The score is continuous and non-increasing in distance.
* The unmapped bonus is bounded (at most x1.25: it never outranks a light more than 25 % more
  important) and **continuous**: it scales with `1 - nativeWeight`, where the weight is the slot fade
  times the native-map validity at the light (the hand-over weight of section 4.3, evaluated at the
  light's view depth). Acquiring a slot (the fade runs 0 -> 1), losing it, and a light crossing the
  native shadow distance therefore move the score smoothly; the earlier binary flag dropped the score
  25 % the instant a slot was acquired, even at fade 0.
* Order: descending importance, then the stable emitting-object UUID. The native map is **reported**
  (`shadow_map`) but no longer ranked ("unmapped first" is gone). Step 1 rules are unchanged:
  suppressed lights are excluded before top-K, per-light work is planned from the requested K, mirror
  lights are excluded, actor-only handling and the count-before-filter production semantics stay, and a
  mapped projector, once admitted, keeps the step 2 "SL map + contact" composition.
* Take records log the importance as `score` for each selected light (at most eight entries), next to
  `shadow_map` and `candidates`.
* The "Shadowed lights" row help (and the setting comment) no longer says leaking lights get priority:
  "Choose up to eight lights. The most important lights (bright, large, close) get soft shadows."

Proof (alrelightmath tests 26, 27 and 33, plus `relight_rank_check.py` source mutations):

* Eight dim unmapped points (luminance 0.3, range 7.5, 3.4-6.2 m) plus one bright mapped world projector
  (luminance 4, range 15, **18 m away, outside its own range, highest UUID**): the projector is admitted
  and ranks first at K=4 and K=8. Restoring unmapped-first, and restoring the camera-point legacy score,
  each drop it at both K values: both mutants fail.
* The same nine lights in all 362,880 input orders, K=4 and K=8: identical selection, order and scores;
  identical inputs give identical output call after call.
* Dolly 0-40 m past six lamps (K 2/3/4, 4,000 steps of 1 cm): importance had 9 selection changes, **0**
  between visibly different lights (reference importance within 2 %); the camera-point legacy score had
  17 changes, all 17 between visibly different lights (mutation rejected). Importance is also verified
  step-free and positive at every one of 40,001 distances.
* Slot fade in with no top-K eviction pop (test 33): K=4, three strong lights, a mapped light whose slot is
  acquired and fades 0 -> 1 against an unmapped neighbour that sits between its scores. The continuous
  bonus changes the selection once, at the crossing, between lights whose scores differ by under 1 %; the
  old binary bonus swaps them at the moment of acquisition with an 11 % gap (mutation rejected). The score
  at fade 0 equals the unmapped score and falls smoothly (at most 0.03 % per 0.001 of fade) to the mapped score.

### 2. Stack review fixes

* **P1 stale "mapped" projectors.** A slot now counts as mapped only if `RenderShadowDetail > 1`, the slot
  is below `bdmergeMaxSpotShadows()`, and its map was **rendered this frame and is complete**
  (`LLPipeline::spotShadowMapSampled`; the Prism aux map under a feed). Two read-only per-slot frame
  stamps are written right after each map renders (`generateSunShadow`, `generatePrismSpotShadows`);
  they never feed native slot upkeep, allocation or auction (the §10 rule holds). Otherwise the light is
  unmapped: contact 0, fade 0, full soft-shadow trace. The pure rule is `ALAreaShadow::nativeMapSampled`.
  Regression (contact fixture, slots 0/3/9, plus the math test): a projector that held a slot keeps
  its full soft shadow through detail 2 -> 1 -> 0 -> 1 -> 2, above a lowered slot limit, with an
  incomplete map, with a stale frame stamp and with a stale Prism aux map; the classification mutation
  (stale owners count as mapped) is rejected.
* **Second-plane churn.** Plane 1 stays allocated while the requested cap is > 4 (allocated at the first
  fifth light, drawn only when used); the dispatch fixture shows a fifth light leaving and returning
  allocates and releases **nothing**. Mutation `spare_plane_churn` is rejected. Allocation failure
  handling is in section 4.1 (no backoff: a failure is retried on the next frame).
* **Logging.** The 1 Hz selection line is now `LL_DEBUGS("RelightQ")`; the budget clamp line is INFO
  only when the clamp state flips (active <-> released), DEBUG otherwise (it previously logged on every
  plan change because of a stale `previous[0]` test); the rig's per-update projector dump
  (`alcinelightrig.cpp`, "light i id= ... noShadow=") is `LL_DEBUGS("CineRigShadow")`, still behind
  `MachinimaDebugLogging`. The allocation-failure warning logs once per failure transition per pack.
* **Housing exclusion**: round 1 made it per light (the emitting prim's oriented box, section 4.2) and round 2 capped it
  (section 5.1). **Round 3 reverted it completely**: housing is again HEAD's global Light bias sphere (section 7.1).
* **Scene allowlist.** `RenderAreaShadowLights` and `RenderAreaShadowMappedContact` join the Director scene
  allowlist (sparse `has()` load, like the others).
* **`multiPointLightF` loop: nothing to change.** `gDeferredMultiLightProgram[count-1]` is compiled with
  `LIGHT_COUNT == count` (one program per batch size, `pipeline.cpp` fullscreen-lights batch), so the
  loop already runs the active count; a runtime bound would only change a stock shader without benefit.
* **Budget ceiling (not measured).** The hard ceiling is the existing 1500M iterations/frame (750M per
  draw), preserved. One iteration is a point depth fetch, a view-space reconstruction and a compare
  (about 50 ALU ops). **Estimate, not a measurement** (no viewer build, no GPU capture): on the RTX 5090
  the full ceiling is roughly 1-5 ms/frame (ALU about 0.7 ms and texture about 0.9 ms ideal, 3-5x for
  divergent loops and cache misses), plus the small filter passes; a GPU with a tenth of that throughput
  could spend 10-50 ms at the same ceiling, which is why `RenderAreaShadowBudget` stays the user-visible lever.
  The Codex review's own 1080p, half-resolution, Thin/K4 gather-only probe measured 6.38 ms on the RTX 5090,
  above that estimate: treat the estimate as optimistic until an in-world timing pass (`mAreaShadowGPUms`
  already measures it). A per-pixel cap was **not** implemented: it would change the OFF plans, and HEAD OFF
  plan identity is a tested promise (`relight_vcam_check.py` compares the planner to HEAD).

### 3. Small items from the Thin objects reviews

* **Feed share with the VCam switch off.** The main view passes `feedSlots = 0` when
  `RenderAreaShadowsVCam` is off (test 29): no reservation (it was 33.55M, 11.2 % of the budget at
  300M/1080p/K4). Zero slots plan against the whole budget, equal the unreserved plan, and never plan
  below the reserved one; Thin OFF plans ignore the slot count (grid: 4 presets x K 1/4/8 x 1080p/4K x
  5 budgets).
* **Stale feed record.** Every area take now carries the `frame` it was sampled on, and the record has a
  `frame_at_start`, so a feed record from long before the take is recognisable.
* **Tests 24/25 tautologies replaced.** Test 24: plan cost recomputed from the target size, the degradation
  order (far steps thin only at 1 ray and half resolution), the common far floor, and literal per-preset
  sample rows (1080p K4 4/8/8/8, K8 4/4/4/4; 4K K4 2/2/2/2, K8 1/1/1/1). Test 25: the feed cost, requested-K
  reservation, draw bound and channel count are recomputed independently from the target size and the
  test's own allowance. (Feeds are deliberately **not** compared across presets: a feed gets what its
  preset's main plan leaves, so a cross-preset feed monotonicity would be a false invariant; it failed
  when tried.)
* **Native projector shadow cutoff** (likely the "environment lights lost soft shadows" report).
  `shadowUtil.glsl` `sampleSpotShadow` returns lit beyond `shadow_clip.w`, and ramps the native shadow to
  lit over the last half of `[clip.z, clip.w]`, while the soft side kept such a mapped projector
  contact-only. The gather receives the same clip (`nativeSpotShadowClip()`, the value `uploadShadowUniforms`
  uploads) as `area_native_clip` and hands the mapped light over to the full trace; the redesigned,
  continuous hand-over is section 4.3. Native allocation and auction untouched.

### 4. Fix round 1 (Codex P0 x2 / P1 x2, GLM, Opus)

#### 4.1 No allocation backoff (Codex P0, GLM P1)

`ALAreaShadow::AllocBackoff`, `kAllocRetryFrames` and `LLPipeline::mAreaAllocBackoff` are **removed**: rendering
no longer depends on failure history (the 300-frame window made two identical requests render differently
depending on a failure 299 frames earlier, and one pack's success cleared another pack's failure).

* A failed allocation retries on the **next frame** (the pre-change behaviour). **Plane 0 failing** releases
  plane 0's targets, gives no soft shadows this frame, and the pass returns (`mAreaShadowValid` stays false);
  an already-allocated plane 1 is **left alone** (round 2, section 5.4) so the next frame does not rebuild it.
  **Plane 1 failing** releases only plane 1 and renders plane 0: the four most important lights keep their soft
  shadows and the others have no channel (`ALAreaShadow::drawablePlanes` / `drawableChannels`; the consumer
  binds plane 1 only while `mAreaShadowCount > 4`, so it is never bound). The take status is `rendered_plane0_only`.
* One warning per failure **transition**, per pack: `RenderTargetPack::areaAllocFailed` is log state only,
  never read by rendering, so main and every VCam feed pack are independent (this is also the "shared per-view
  state must be per-pack" item; the remaining statics in the pass, `previous`/`previous_clamped`/`summary`, are
  main-view-only logging and are guarded by `!sPrismLensRender`).
* Tests: the old backoff test is gone. Dispatch fixture (production code, real GL): for a K=8 feed every one of
  the six target allocations is failed in turn: plane-0 failures leave nothing live and render nothing, plane-1
  failures keep plane 0 (3 targets) and render four lights, and the very next frame recovers all six targets.
  Persistent failure for five frames (plane 0 never allocates; only plane 1 never allocates) retries the
  allocation **every frame**, and the first frame with memory is **byte-identical to a fresh frame** (history
  independence). Test 32 pins the policy and replays the superseded 300-frame model to show it renders differently
  for equal inputs. New dispatch mutations rejected: `sticky_failure` (a history-dependent early return) and
  `plane1_failure_drops_all`; `no_alloc_backoff` / `failure_not_recorded` no longer exist.

#### 4.2 Housing is the emitting prim's oriented box (Codex P0, Opus P1)

> **SUPERSEDED in round 3 (section 7.1).** Everything in this subsection (the `area_box` uniform, `housingBox`,
> `areaInHousing`, `areaBoxEndGap`, `area_housing_box`) was removed; housing is HEAD's global bias sphere again. Kept
> as the record of what was tried.

The bounding-sphere radius (`0.5*|scale|`, capped 1.5 m) is gone (`area_housing_radius`, `housingRadius`,
`kHousingCap`, the `area_housing` uniform). It rejected every occluder inside the sphere and shortened every
ray by it, so a 0.02 x 3 x 0.02 m emitting strip erased any slab within 1.5 m of its centre (Codex measured
minimum visibility 0.000 -> 1.000 on the production gather).

New model (`area_housing_box` in `pipeline.cpp`, `ALAreaShadow::housingBox`, `areaInHousing` /
`areaBoxEndGap` in the shaders), per channel, this frame's transforms only:

* **Per-light data**: `area_box[4n..4n+3]` = prim centre + active flag, then each unit view-space axis with its
  half extent (`0.5 * scale`, capped 1.5 m since round 2; it was 32 m). Runtime uniform, no shader variant.
* **Rejection**: an occluder (and a validated hit) is ignored only if it lies inside the prim's oriented box
  **expanded by the light bias** (`|local_k| <= half_k + bias`). The box expanded by the bias contains the old
  bias sphere, so the housed wall lamp is never exposed more than before.
* **End gap**: the ray stops where it enters that expanded box (slab test in the oriented frame), never
  shorter than the source floor `max(source radius, 0.1 m)`; a ray that never enters the box keeps the floor.
* **No box (flag 0)**: prims with **no opaque face** (every face absent, or in an alpha pool: an invisible light
  prim writes no depth, so it cannot shadow itself) and **rig fixtures / catchlights** (synthetic emitters)
  keep the committed arithmetic exactly: bias sphere, `end_gap = max(source radius, 0.1) + bias`. A rig
  fixture's source radius is therefore counted **once** (the uncommitted change had added it as housing
  as well, double counting it).
* Regressions on the production gather (`relight_rank_check.py`) with CPU twins in test 31: a 1 m cube
  "lightbox" no longer shadows its own light (min 0.000 -> 1.000); a 0.02 x 3 x 0.02 m strip keeps an
  independent slab 0.5 m beside it (min stays 0.000; the superseded sphere erased it, shown with a
  cube stand-in); a 2 x 2 m panel is shadowed by a blocker 0.8 m below it (min 0.000; erased by the old sphere);
  the housed wall lamp keeps its pool with its bulb's box; a rig softbox of radius 1 / 1.5 / 2 m on a 3 m orbit
  stays a soft shadow (dark and lit regions plus more than eight partial pixels), and in the CPU twin the
  double-counted housing removes it (mutation rejected). The box is oriented (rotated-prim cases), per
  channel, and zero flag = HEAD bytes.

#### 4.3 Native hand-over (Codex P1, Opus P2)

The old hand-over scaled the mapped light's contact fade by `1 - clamp(2u-1, 0, 1)`, `u = (depth - clip.z) /
(clip.w - clip.z)`, which is the **same half** in which `sampleSpotShadow` ramps the native shadow to lit. For a
blocker both systems see, native visibility rose as `t` while the screen visibility fell as `1 - t`, so the
final `min()` transmitted 50 % light mid-way between two dark endpoints (reproduced on the production gather;
the earlier sweep only compared the interpolation with itself).

* **Design**: the screen term now takes over across `u in [0.25, 0.5]` (`kHandoverStart/End`; **round 3 ends it at
  0.45**, section 7.2), finished **before** the native ramp starts at `u = 0.5`. Inside the window the native value is still its own shadow
  and beyond it the screen visibility is the full trace, so for a shared blocker every term of
  `min(native + shadow_fade, screen)` is dark across the whole transition, while a native-only blocker
  keeps the stock ramp to lit and a screen-only blocker follows the continuous hand-over to the full trace.
  Unset or malformed clips (GL default 0,0) are "valid everywhere" (HEAD bytes).
* **Independent combined-output oracle** (`relight_rank_check.py`, GPU): the production gather feeds the
  production spot-light composition (`spotLightF.glsl`'s own `min(shadow, areaShadowVisibility)` and real
  `sampleSpotShadow`, linked as in the contact gate), swept over 256 receivers 4-9.9 m deep for 3 clips
  (including one where the cascade-blend weight `W` is below 1 while the ramp runs), 3 fades and 4 scenes
  (shared blocker, screen-only, native-only, nothing): the gather follows the strength within 3/255, the
  composition equals `min(native model + shadow_fade, screen)` within 3/255 (the model includes the
  `/weight` term, `ALAreaShadow::nativeSpotShadow`), no step exceeds 1.5x the hand-over slope, and a shared
  blocker is **never brighter than the brighter endpoint** (peak 0.000; the superseded hand-over peaks at 0.427).
  CPU twin: tests 28 and 30 (1.47M samples over 5 clips x 5 fades x 5 scenes).
* **Unmapped bonus** is now continuous (section 1, Opus P2); the regression is test 33.

#### 4.4 Other

* Opus P2 shared per-view state: the only one was the allocation backoff (removed above).
* No §10 change: native slot upkeep, allocation and auction are untouched; no shader permutation or reload
  dependency was added (`relight_off_check.py`), the new gather uniforms are runtime-only, and the uniforms
  declared in both linked gather files are intentional.

### 5. Fix round 2 (Opus P1, Codex P1, DeepSeek, GLM)

No reviewer found a P0. Items in the brief's order.

#### 5.1 A large emitting prim no longer erases its own room (Opus P1, treated as a blocker)

> **SUPERSEDED in round 3 (section 7.1).** The capped box and its per-ray fallback were removed with the rest of the
> per-light housing; the room-prim cases survive as regressions against the global sphere.

With `kHousingMaxHalf = 32 m`, a light on the root of a mesh house, a hollow room prim or a sky dome (8 x 8 x 3 m,
light at the centre) rejected every blocker inside the box **and** gave any receiver inside the box `tmin < 0`,
so `end_gap = length`: a zero-length ray, fully lit. A receiver within 0.25 m of a wall or ceiling emitter prim
was never shadowed either. Three rules, each with a CPU twin and a GPU regression:

* **(a) Cap**: `kHousingMaxHalf = 1.5` (the old sphere's cap). The 3 m strip (half 1.5) and the 2 x 2 m panel
  (half 1.0) are untouched; an 8 x 8 x 3 m room gets a 1.5 m cube box (`insideHousing` at 3 m from the light is false).
* **(b) Receiver inside the expanded box uses the bias sphere for that ray.** `areaShadowF.glsl` main tests the
  ray origin once per light (`areaInHousing(origin, ...)`) and, if inside, clears the box flag for that light's rays:
  the occluder test is the committed `length(p - light) <= bias` and `end_gap = max(source radius, 0.1) + bias`.
  CPU mirror: `insideExpandedBox`, `housingForRay`, and `housingEndGap` returns `floor + bias` for an origin inside
  (the old "whole ray is housing" expectation in test 31 is replaced by `.35`; that is the rule change, not a
  loosened check).
* **(c) No box for rigged drawables**: `area_housing_box` returns before reading any transform when
  `volume->isRiggedMesh() || drawable->isState(LLDrawable::RIGGED)` (prim scale and position do not match rendered
  geometry, and `getRenderRotation` is identity for rigged), `housingBox(..., rigged)` gives an inactive box.
* Regressions (`relight_rank_check.py`, production gather, GPU): an 8 x 8 x 3 m room prim (box half extents 1.5)
  with the receivers inside it: the slab shadows (min 0.000) and, linked without the per-ray fallback (mutation),
  the same fixture is fully lit (1.000); a 1.2 x 1.2 x 0.2 m sconce 0.25 m off the wall: the wall beside it is
  shadowed **exactly like the bias sphere** (mean difference < 1/255) by a slab 0.14 m above the wall, and lit without the
  fallback. The 1.25 m cube and 1.17 m cube fixtures that stood in for the superseded sphere now do so through that same
  no-fallback mutation (the receivers sit inside those boxes). CPU twins (test 31): the same room with a prop,
  a receiver beside a ceiling panel prim, rigged = no box, no opaque face = no box. Static source checks
  pin the rigged early-return order, the cap and the `if (box inside) sphere` text, each with a rejected mutation.

#### 5.2 Hand-over at the native sampler's shifted receiver depth (Codex P1)

`sampleSpotShadow` first moves the receiver, `pos += norm * spot_shadow_offset`, and ramps to lit by **that** depth.
The gather evaluated the hand-over at `-pos.z`, so a tilted receiver met the native ramp mid-way: Codex measured
a bright band peaking at 10/255 (default offset 0.04 m) and 40/255 (UI maximum 0.15 m) on clip (4, 5) with the
normal (0.707, 0, -0.707).

* `areaShadowF.glsl`: `native_strength = areaNativeStrength(-(pos.z + normal.z * area_native_offset))`, with the
  runtime uniform `area_native_offset` = `RenderSpotShadowOffset` (the value the stock upload sends; non-finite sent as 0).
  CPU mirror `ALAreaShadow::nativeReceiverDepth(viewZ, normalZ, offset)`.
* **Oracle** (`relight_rank_check.py`, combined output): normals (0,0,1), (0.707,0,-0.707), (0.707,0,0.707) x
  offsets {0, 0.04, 0.15} (and -0.04 / -0.15 for the facing normal: the same deeper shift without starting the ray behind
  its own surface; a back-facing normal self-hits in this fixture) x 3 clips/blends x 3 fades x 4 scenes through the
  production gather **and** the production spot-light composition, with the independent native model fed the shifted
  depth. The gather follows the shifted strength within 3/255, the composition equals `min(native + fade, screen)`,
  and a shared blocker's peak is **0.000** at fade 1 for every offset and normal. The fixture's `area_trace_res`
  is now production-like (the 2 cm receiver-bias floor; the 256 x 8 grid's footprint was ~0.4 m).
  Mutation: the same gather linked with `-pos.z` peaks at 0.039 (0.04 m) and 0.149 (0.15 m) on the tilted
  clip (4, 5) case. Gather level (d2): offsets 0.04 and 0.15 follow `native_strength(depth - offset)`; the unshifted
  gather is off by more than 12/255.
* CPU twin: test 30 (receiver-offset oracle over 4 clips x 3 offsets x 5 normals x 2 fades: peak 0 for the shifted form,
  0.23 for the unshifted one) and `nativeReceiverDepth` literals.

#### 5.3 Ranking weight through validity loss (DeepSeek P1): documented, not changed

`native_weight = fade * strength` while the slot is sampled, `0` when `spotShadowMapSampled` flips false
(shadow detail lowered, slot lost to the live limit, map not rendered this frame). That one-frame drop of the
25 % bonus is real. Carrying the old fade through the flip needs the previous frame (history, which this ranking
forbids) and would rank a light by a native shadow it no longer has (its contact fade and trace are already
the full unmapped form on that frame). It only happens on a user action or a lost slot, never from camera or light
motion, so it is documented at `ALAreaShadow::importance` as the one remaining step rather than papered over.

#### 5.4 Plane 0 failure keeps an allocated plane 1 (GLM P2)

`if (!plane_ok[0]) break;` replaces `release_plane(1); break;`: when plane 0 fails, an already-allocated spare plane is
neither built nor freed, so the next frame rebuilds plane 0's three targets only. Dispatch fixture: a K=8 feed/main with
both planes live, a resize whose plane 0 allocation fails (plane 0 released, plane 1 kept: live textures -3), and back at
the old size exactly 3 allocations restore it. Mutation `plane0_failure_frees_spare` is rejected;
`plane1_failure_drops_all` now flips the plane-1 failure into a plane-0 failure.

#### 5.5 Test 26 (GLM P2)

362,880 permutations x 2 caps x Selection inserts became the identity, the reverse and 5,000 fixed Fisher-Yates
shuffles (own xorshift32 stream; `std::shuffle` is implementation defined). Power is kept: the comparator is a strict
total order (score, then UUID), the equal-score fixture of four lamps is checked in **all 24** orders, and a
comparator without the UUID tie break is shown to be order dependent on that fixture (mutation rejected).

#### 5.6 Unit axes (DeepSeek P2)

> Moot after round 3 (section 7.1): `housingBox` no longer exists.

`housingBox` **normalises** its axes (a scaled axis gives the same box) and returns no box for a zero-length or non-finite
axis. The production caller already passes unit axes; this makes the pure function safe on its own.

#### 5.7 Slot fade-in transient (Opus P2, optional): documented, not changed

At slot fade `f` = 0.5 a shared blocker still reads about 50 % through the composite (`min(native + (1 - f), screen)`,
`screen = f * s`). The fix named in the brief, `mix(full, min(native_unfaded, contact), f * s)`, needs the full-trace and
contact visibilities separately in the spot shaders, but the gather packs only their mix into one channel. Splitting
them means a second visibility channel set (memory, bandwidth and a consumer change in three lighting shaders), which is
not trivially safe. The transient exists at HEAD and only happens while a slot fades in, so it is left.


### 6. Gates of round 2 (historical; the current gates are section 7.5)

| Gate | Result | Log (`.tmp/rank_r3/`) |
|---|---|---|
| `relight_shader_check.py` (full: production shaders, OFF bytes, rig, area, contact, thin, housing box and room/sconce regressions, shifted-depth hand-over, combined oracle) | **PASS** | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` (policy, VCam CPU/dispatch fixtures with 16 mutations, contact CPU, thin, rank) | **PASS** | `final_area.log` |
| `relight_thin_check.py` direct; `relight_rank_check.py` direct (`--rank-only`); `relight_off_check.py` (OFF identity harness) | **PASS**; **PASS**; **PASS** | `final_thin_direct.log`, `final_rank_direct.log`, `final_off.log` |
| Standalone `alrelightmath` tests | **PASS 33/33** (30 gains the receiver-offset oracle, 31 the room / panel-neighbour / rigged / unit-axes cases, 26 sampled permutations plus the 24-order tie fixture) | `math.log`, `math_compile.log` |
| `zs_check.ps1 -Files <one file>` | **PASS** `pipeline.cpp`, `llfloaterdirector.cpp`, `alcinelightrig.cpp`, `llviewercontrol.cpp`, `tests/alrelightmath_test.cpp` (separate calls) | `zs_*.log`, `zs_rc.txt` |
| Three generators `--check` (`director_page_sources.py` regenerated first: it pins `pipeline.cpp` citations) | **PASS** x3 | `director_kit_gen_check.log`, `director_page_sources_check.log`, `director_functional_manifest_check.log` |
| `prebuild_guard.ps1 -ChecksOnly` (`GIT_CONFIG_COUNT=1`, `core.safecrlf=false`) | **PASS**, all 11 gates, exits before the object purge | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Key numbers (production gather + production spot-light composition, RTX 5090): shared-blocker peak **0.000** at every
receiver offset (0 / 0.04 / 0.15, and -0.04 / -0.15) and normal; the unshifted hand-over peaks 0.039 / 0.149 on the
tilted clip (4, 5) case (the superseded round-1 form 0.427); an actor inside the 8 x 8 x 3 m room prim keeps its
shadow (min 0.000; 1.000 without the per-ray fallback). Static checks pin eleven source mutations; the VCam dispatch
fixture rejects all 16 of its mutations, including `plane0_failure_frees_spare` and `native_offset_dropped`.

Logs are in `.tmp/rank_r3/`. New file to include in the commit: `scripts/code_tools/relight_rank_check.py`
(also run by `relight_area_check.py`; it re-launches the GPU harness with `--area-only --rank-only` when run
directly). Updated gate fixtures: `relight_vcam_dispatch_check.py` (adds the receiver-offset upload, the
spare-plane-survives-plane-0-failure scenario and two mutations), `relight_thin_check.py` (the review-HEAD shim also
supplies the sphere-only `areaInHousing` the current gather now calls), `relight_contact_check.py` (its rejected
length-interpolation variant re-declares `areaInHousing`). Not measured: whole-viewer image identity, in-world
quality, GPU timing, and the real `shadow_clip` / `spot_shadow_offset` values in a live scene.

### 7. Fix round 3 (final): per-light housing reverted, hand-over margin, small fixes

Inputs: the Codex, GLM, DeepSeek and Opus reviews of round 2. No commit, push, stash, viewer build or viewer launch;
`.tmp/ref_private/` and ReShade/iMMERSE files were not opened. Everything is runtime-only and deterministic.

#### 7.1 Housing is HEAD's again (decision by the lead)

Three designs of per-light housing each introduced a new visible artefact: the half-diagonal sphere erased blockers; the
oriented box wiped actor shadows inside room prims, drew a hard seam at the box edge and a dark ring around sconces; the capped
box lost blockers just outside the cap (Codex GPU repro on the production gather: light (1, 0, -2.5), receivers z = -4, x < -.78,
independent blocker at z = -3, visibility 0.000 -> 1.000 at 4/8, 8/12 and 16/64 rays/steps). So housing is again **only** the global
Light bias sphere (`RenderAreaShadowLightBias`) and HEAD's end gap `max(source radius, 0.1) + bias`:

* Removed: `area_box`, `areaInHousing`, `areaBoxEndGap`, `area_housing_box` (pipeline.cpp), `HousingBox`, `housingBox`,
  `housingForRay`, `housingEndGap`, `insideExpandedBox`, `insideHousing`, `kHousingMaxHalf`, the opaque-face counting, the box
  upload and the per-ray fallback. `screenSpaceRayUtil.glsl` is **byte-identical to HEAD** and the gather's housing lines are
  HEAD's verbatim (`relight_rank_check.py` compares both against `git show HEAD:` and bans every removed name in all five files).
* The rig source radius is counted **once**, as at HEAD (end gap `max(radius, 0.1) + bias`; nothing is added as housing).
* **Documented known limitation:** a LARGE emitting prim (a lit room shell, a big softbox, a sky dome) can shadow its own light.
  The remedy is the user raising **Light end bias** on the Soft shadows card; its help text (kit row, `settings.xml` comment and the
  regenerated kit files) now says so: "Raise it if a lamp, or a large glowing prim such as a lit room shell or big softbox, shadows
  itself; large values can miss nearby blockers."
* Tests: the housing-box tests (math test 31, the box GPU cases, the dispatch fixture's `area_housing_box` stub, `housing_dropped`
  mutation and `area_box` upload assertions, the `areaInHousing` shims in the thin and contact gates) are gone. Kept: the housed
  wall lamp (CPU + GPU pool stays lit with HEAD's sphere), the rig softbox radius 1 / 1.5 / 2 m staying soft, and Codex's room-prim
  cases as regressions that pass with the sphere: independent blockers shadow receivers **near** the light (inside what the capped box
  covered) and **far** from it (outside), at 4/8, 8/12 and 16/64 rays/steps on the GPU (min visibility 0.000 in both), and in the CPU
  march. Discriminating fixtures: an oversized housing (a 1.5 m sphere) erases the far blocker (visibility 1.000 on the GPU), and with
  no housing at all the wall-lamp casing shadows its own bulb.

#### 7.2 Hand-over margin (Opus P2)

`areaNativeStrength` reached 0 exactly at u = 0.5, where the native ramp starts, and the bilateral filter (radius up to 8 px) leaks a
few /255 across it. The screen hand-over now ends at **u = 0.45** (`kHandoverStart/End` = .25 / .45 in the header, `(u-0.25)*5.0` in the
shader; each cross-references the other, and `relight_rank_check.py` pins header, shader, its own mirror and the C++ test).

The combined-output oracle now runs **with the production filter**: the production gather -> blur x -> blur y -> upsample chain at radius 2
(the default `RenderAreaShadowDenoise`) and 8 (the maximum), then the production spot-light composition, for every normal / offset /
clip / fade / scene (1188 sweeps). Results:

* Shared-blocker peak **0.000** (<= 1/255) for every clip range >= 2 m at any radius, and for every range at radius <= 2.
* **Exception, measured:** a 1 m clip range (4, 5) at radius 8 peaks at **2/255** (two columns of margin only); the oracle bounds that
  combination at 3/255, the previous gate. A realistic native shadow range is far larger, and the default radius is 2.
* Mutation: the previous window (ending at u = .5) peaks 6/255 through the radius-8 filter and is rejected; the superseded hand-over
  (with the native ramp) peaks 0.427.
* The margin heals a receiver shift of up to 0.05 of the clip range, so the unshifted-depth mutation is now only visibly wrong on the
  0.15 m maximum offset (peak 0.071 on a 1 m range) or on a 0.4 m range at 0.04 m (peak 0.063); both are still rejected, with the fixed
  gather at <= 1/255 (<= 3/255 on the 0.4 m range). The earlier floors (5/255 and 20/255 on the 1 m range at 0.04 and 0.15 m) were
  replaced by these because the new margin removes most of that leak by design.
* Raw-gather checks are unchanged (follows the strength within 0.92/255). A filtered column is checked to lie within the range of the
  unfiltered curve over +-radius columns (the blur is a convex combination), instead of the raw curve, which smoothing rounds on purpose.
* C++ test 28: window literals at u = .25 / .35 / .45, `1/(End-Start) = 5`, the margin `>= .05`, and strength 0 wherever the native ramp rises.

#### 7.3 Small fixes

* GLM P2: `break;` after `native_fade = mSpotLightFade[i];` in the slot loop (a drawable owns one slot; the first sampled one decides).
  Pinned by a source check with a mutation.
* DeepSeek P2: `areaNativeStrength` returns 1.0 for a non-finite depth (`isnan || isinf`), matching the CPU `isfinite` rule
  (`nativeShadowStrength`; NaN / +Inf / -Inf pinned in test 28; the shader guard and its mutation are pinned in the rank gate).

Kept unchanged: importance ranking, mapped validity, native-cutoff hand-over at the shifted depth, continuous bonus, plane retention,
no backoff, logging, scene allowlist, feed reservation and the take-record fields. No Section 10 change: native slot upkeep, allocation and
auction are untouched; the budget is unchanged. OFF is byte-identical (the OFF harness passes; the ray utility is HEAD's, the gather
differs from HEAD only by the two runtime hand-over uniforms and their strength term).

#### 7.4 Not measured

Whole-viewer image identity, in-world appearance of a large emitter with Light end bias raised, GPU timing.

#### 7.5 Gates (each run once after the final edit; GPU gates one at a time; logs in `.tmp/rank_r4/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full) | **PASS** | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` (incl. rank; VCam dispatch rejects all 15 remaining mutations) | **PASS** | `final_area.log` |
| `relight_thin_check.py`; `relight_rank_check.py`; `relight_off_check.py` | **PASS**; **PASS**; **PASS** | `final_thin_direct.log`, `final_rank_direct.log`, `final_off.log` |
| Standalone `alrelightmath` tests | **PASS 33/33** | `math.log`, `math_compile.log` |
| `zs_check` per touched .cpp (pipeline, alrelightmath_test; llfloaterdirector, alcinelightrig, llviewercontrol too) | **PASS** x5 | `zs_*.log`, `zs_rc.txt` |
| Three generators `--check` (kit regenerated first for the help text, page sources regenerated for the shifted `pipeline.cpp` citations) | **PASS** x3 | `director_kit_gen_check.log`, `director_page_sources_check.log`, `director_functional_manifest_check.log` |
| `prebuild_guard.ps1 -ChecksOnly` (`GIT_CONFIG_COUNT=1`, `core.safecrlf=false`) | **PASS**, all gates | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Files to include in the commit beyond round 2: none new (`scripts/code_tools/relight_rank_check.py` is still the new file). Reverted to
HEAD content: `screenSpaceRayUtil.glsl`, `relight_thin_check.py`.

## World shadows Phase 0 (10/22)

Phase 0 of `RELIGHT_WORLD_SHADOWS_PLAN.md` (Revision 2): grain and acne fixes for the CURRENT screen-space soft shadows. Runtime uniforms only
(no define, no reload), deterministic (no frame/time input), no change to native shadow allocation, the auction or count-before-filter,
Soft shadows OFF byte-identical (the OFF harness and `relight_off_check.py` pass). User decisions applied: ray floor 4 (fewer lights, smooth),
P0 may change the default look, Thin objects keeps its 48 near + far floor and its railing/join/feed guarantees, P0.6 not redone.
Branch `feature/director-kit-p0`, base `4dd9ff661c1` (per-light housing stays the global Light-bias sphere). Nothing committed, built or stashed.
This section is the state after **fix round 3** (reviews: Codex, Opus, GLM, DeepSeek, three times). Section 8 lists what round 1 changed; section 9 what round 2 changed (four user decisions of 10/22: cost, accuracy, Thin hybrid, smooth fade); **section 10 what round 3 changed** and supersedes sections 9.3 (the hybrid's regular group, the feed text), 9.4 (the fade floor) and 9.5 (the coverage table) where they differ. The tables below are brought up to date, the earlier values are kept where they still hold.

### 1. What changed, per item

| Item | Change | Files |
|---|---|---|
| P0.1 planner ladder | `workloadWithin` runs a fixed integer ladder: (1) full resolution only while 12 rays still fit, else half; (2) rays down to 8; (3) steps down to max(8, steps/2) (Thin: the far segment, same rule); (4) rays down to 4; (5) steps down to 8 (Thin: the common far floor `min(minFar, 8)`, near stays 48); (6) lights: A falls one at a time down to 1. **Below the floors the MAIN view admits no light** (`A = 0`, nothing drawn): never a sub-floor ray or step, and more budget never admits fewer lights. A Thin objects request is planned by `hybridWithin` (sections 9.3 and 10): Thin never admits fewer lights than Thin OFF; the lights the budget cannot cover at Thin quality march EXACTLY the Thin-OFF plan, and while VCam feeds can run a Thin or hybrid main never costs more than the Thin-OFF main. **The OFF (uniform) plan is NOT the pre-P0 plan:** P0.1 applies the resolution rung and the floors to uniform plans too, by design (user decision: "P0 may change the default look"); only Soft shadows OFF is byte-identical. A capture FEED keeps the previous sub-floor degradation (its guarantee depends on it). `Workload.admitted` = A (from the requested cap K, never the visible set); the pipeline keeps the A highest-ranked lights, the rest get no soft shadow. | `alareashadowmath.h`, `pipeline.cpp`, `alrelighttakerecord.h` |
| Admission fade | **Smooth group fade (round 2):** every admitted light fades with `smoothstep(1, 1.05, score[i] / score[first dropped])` (per-channel uniform `area_dropout`, frame-pure, 1 when nothing is dropped; window narrowed from 1.25 to 1.05 by the lead on 10/22; **round 3: never below 0.5 for an admitted light**, section 10.5), so lights that trade places at the coverage boundary fade instead of one popping, and a swap within the admitted set changes no strength (section 9.4). Round 1 faded only the last admitted channel. | `alareashadowmath.h` (`admissionStrength`), `areaShadowF.glsl`, `pipeline.cpp` |
| Status line | Readout row **Coverage** on the Soft shadows card: "Soft shadows on 8 lights" / "Soft shadows on 2 of 8 lights (work budget)" / "No soft shadows right now (work budget | low video memory)" / "Soft shadows are off" (`ALAreaShadow::coverageText`); with Thin objects "Soft shadows on 8 of 8 lights (3 thin-quality)" (the lights that march the Thin objects march; "(0 thin-quality)" when Thin fell back entirely; an explicit low-memory flag from the pass gives "low video memory" on every early return). The pass clears the main-view coverage on entry, publishes the planned count after planning and the count actually drawn after allocation and drawing (a light faded below 1/255 is not counted), so early returns and allocation failures never leave a stale value; the row is bound with its page (`bindPage`). Logged as `admit=A/K` in the existing clamp line. | `llfloaterdirector.cpp/.h`, `pipeline.cpp/.h`, kit data |
| P0.2 noise | Rotation and ray start of every noise cell come from the R2 rank `frac(shift + a1 x + a2 y)` in exact 32-bit integer arithmetic (a1 = 1/phi2, a2 = 1/phi2^2); the ray start uses the lattice turned by 90 degrees (the first version's start lattice made rotation + start depend on x + y alone and printed 45-degree stripes), and the per-light Cranley-Patterson shift (light UUID hash, never the channel) is re-rolled per 8x8 block so no lattice line stays coherent beyond 8 cells. Blur radius is at least 2 trace pixels whenever the blur is on (`ALAreaShadow::filterRadius`; HEAD was 1 at 1080p half-res); zero skips it. The step at the lowest setting is deliberate and documented (a Denoise value that scales below 2 trace pixels reads as 2; the Director help says so). | `areaShadowF.glsl`, `alareashadowmath.h`, `pipeline.cpp` |
| P0.3 filter edge-stops | Geometric normal from depth (min-difference derivative, one trace pixel apart; planarity test with a distance clamp for steep slopes) and a plane-distance weight with a per-tap tolerance (half a footprint at the first tap, growing to 2 footprints with the tap distance, floor 2 cm) replace the raw linear-depth weight; the normal-mapped normal only vetoes taps more than ~60 degrees apart, no longer `pow(dot, 32)`. A one-sided **hand-over stop** (`area_native_clip`/`area_native_offset`, the gather's own window, uploaded for every filter pass) keeps a traced pixel from averaging contact-only taps across the native hand-over band; **per channel (round 2):** the stop scales with each channel's contact fade (`area_contact_fade` goes to the filter too), so a light with no native map keeps its whole kernel across the band, and the all-taps-rejected fallback is per channel (centre tap / lowest hand-over weight among the taps that were on the centre's surface, else the pixel's own texel, never a bilinear mix; round 3). Known limit, measured: a crease only cuts once its plane offset exceeds the tolerance (a lit face and a shadowed face meeting at 30 degrees still blend over about 2 trace pixels; the old normal-mapped stop cut them but left brick and fabric speckled). | `areaShadowFilterF.glsl` |
| P0.4 horizon | **Not adopted.** Radiance reference (deterministic quasi-Monte Carlo, true disk light over the geometric horizon, 48 cases: distance 4/8 m x source 0.1/0.3/1 m x Light shape OFF/ON x flat/tilt toward/away/side normal map): skipping below-horizon rays and renormalizing wins 24 of 48 but with the shading normal tilted 25 degrees toward the light the dark side leaks, worst 23.8/255 against HEAD's 5.8/255. The plan's rule fails, so the gather keeps HEAD's behaviour. Reproducible: `relight_horizon_study.py`, asserted in the grain gate. | `relight_horizon_study.py` |
| P0.5 geometric offset | **Deferred (reverted).** No fixture was found where HEAD's shading-normal receiver offset causes acne that P0.5 cures (normal-mapped blockers and receivers, even with extreme tilts, are clean on HEAD), and the geometric-normal reconstruction in the candidate refinement and validation cost about a third of the gather (1.33-1.37x HEAD at Medium 1080p K4 with it). `screenSpaceRayUtil.glsl` is byte-identical to HEAD again and the gather's receiver offset is HEAD's. Revisit with a real fixture. | (none) |

### 2. Before / after

Grain = RMS (8 light UUIDs) of the final filtered image around the penumbra of a half-plane blocker lit by a 0.3 m disk, /255, one wall
fixture at 1080p and 4K (`relight_grain_check.py`). Pre-P0 = commit `4dd9ff661c1` planner + shaders + radius. The first numbers are the ensemble
spread around the images' own mean (what the gate measured before fix round 1); the second are the gate's current metric, the error against a 64-ray
reference at the plan's own march (it also sees a bias).

| Case | Pre-P0 (spread / error) | P0 before round 1 (spread) | P0 now (spread / error) |
|---|---|---|---|
| 4K Cinematic K8 | 23.5 / 24.8 (1 ray x 22 steps full-res, r=4) | 3.5 | 3.7 / **3.8** (5 rays x 16 steps half, r=2, all 8 lights) |
| 4K Cinematic K4 | 23.1 / 24.8 | 2.6 | 3.1 / **3.1** (8 rays x 22 half) |
| 4K Thin K8 and K4 | 21.1 / 23.1 (K8), 11.7 / 12.4 (K4) | 2.7 | 3.5 / **6.3** (4 rays x 56, 3 lights) |
| 4K Low | 10.4 / 10.4 | 5.2 | 5.9 / **5.9** |
| 1080p Low | 17.5 / 17.5 (radius 1) | 5.7 | 5.7 / **5.8** |
| 1080p Medium K4 | 7.4 / 7.3 | 2.6 | 3.1 / **3.1** |
| 1080p Thin Medium K8 | 11.6 / 11.8 (4 rays) | 2.0 | 3.1 / **4.2** (5 rays x 64) |
| Normal-mapped wall, Medium (7-8 rays x 12) | 8.1 (flat 4.7) | 2.8 | same as flat (gate: <= 1.15 x flat + 0.3) |
| Grazing floor (~87 degrees), Medium | 7.6 | 2.0 | **2.3** |

Worst P0 configuration: 6.3/255 (4K Thin), 5.9 (4K Low); limit 8, Medium 1080p K4 limit 6 (3.1). Pre-P0 fails 12 of the 28 effective configurations. The small rise of the
spread against the earlier numbers (2.6 -> 3.1 at Medium) is the price of the block-wise shift that removes the lattice stripes (section 4, spectrum).
Edge location, contrast and width: against the analytic profile at a converged march (64 rays x 512 steps) the edge is within **1 full-resolution pixel**
(measured 0.09 px at 1080p, 0.65 px at 4K), width within 1 percent of the analytic, umbra 0, lit 255. At the PLAN's own march the edge is set by the march: at 64 steps
it is 4 px off (1080p) and at 8-16 steps tens of pixels, before and after P0 alike (equal plans: mean edge within 1 cm of the pre-P0 shader, 80 rows x 8 seeds).

### 3. Effective coverage (default 1500 M budget, VCam feeds OFF, requested cap K)

Cells: admitted/K lights, rays, steps (Thin: near 48 + far, e.g. 64 = 48 + 16). Columns Low | Medium | High | Cinematic.

| Screen, K, Thin | Low | Medium | High | Cinematic |
|---|---|---|---|---|
| 1080p K4 OFF | 4/4, 4 x 8 | 4/4, 8 x 12 | 4/4, 12 x 20 | 4/4, 22 x 32 |
| 1080p K4 ON | 4/4, 4 x 80 | 4/4, 8 x 80 | 4/4, 9 x 80 | 4/4, 9 x 80 |
| 1080p K8 OFF | 8/8, 4 x 8 | 8/8, 8 x 12 | 8/8, 12 x 20 | 8/8, 11 x 32 |
| 1080p K8 ON | 8/8, 4 x 80 | 8/8, 5 x 64 | 8/8, 5 x 64 | 8/8, 5 x 64 |
| 4K K4 OFF | 4/4, 4 x 8 | 4/4, 8 x 12 | 4/4, 9 x 20 | 4/4, 8 x 22 |
| 4K K4 ON | **4/4**: 3 Thin 4 x 57 + 1 regular 4 x 8 | 4/4, 8 x 12 (the OFF plan, 0 thin-quality; round 4: no Thin light fits at the regular 8 rays) | 4/4, 9 x 20 (the OFF plan, 0 thin-quality) | 4/4, 8 x 22 (OFF, 0 thin-quality) |
| 4K K8 OFF | 8/8, 4 x 8 | 8/8, 8 x 11 | 8/8, 8 x 11 | 8/8, 5 x 16 |
| 4K K8 ON | **8/8**: 2 Thin 4 x 66 + 6 regular 4 x 8 | 8/8, 8 x 11 (OFF, 0 thin-quality) | same | 8/8, 5 x 16 (OFF, 0 thin-quality) |

**Fix round 4:** a hybrid's Thin lights march the regular ray count (section 11.1), so 4K K4 Medium, which had 2 Thin lights at 4 x 64 beside regular 8 x 12 in round 3, is now the OFF plan; every other cell is unchanged. VCam feeds are planned from the feeds LIVE that frame (section 11.4); the "with VCam feeds ON" sentence below is the round-3 worst case (eight assumed 2048 x 2048 feeds) and the live-feed table is in section 11.4.

**Fix round 3:** the regular lights march exactly the Thin-OFF plan of their preset, and the Thin lights get only what that leaves, so where the OFF plan already uses the budget (4K High and Cinematic, 4K K8 Medium) Thin ON is the OFF plan and the row says "0 thin-quality" (round 2 had squeezed the regular lights to 4 x 8 there to give the top lights the Thin march: 4K K4 Cinematic's fourth light 8 x 22 -> 4 x 8, grain about 3.5 -> 5.9/255). With VCam feeds ON (the VCam switch) a Thin or hybrid plan is also capped at the Thin-OFF cost (section 10.1): 1080p K4 and K8 keep Thin for High and Cinematic only (4 x 60 and 8 x 80 / 5 x 64), Low and Medium are the OFF plans; at 4K every preset is the OFF plan.

Half resolution everywhere (a full-resolution request survives only where 12 full-res rays fit, which no 1080p or 4K row allows). The table is for VCam feeds OFF. The full grid
(720p, 1080p, 1440p, 4K x K1-8 x Thin x feed reservation, 512 rows) is re-derived by an independent closed-form ladder in the gate and printed in `final_grain.log`.
The admitted lights are preset-independent where the floors bind (round 1 admitted 3 lights at 4 rays x 56 steps at 4K Thin; rounds 2 and 3 keep every OFF light), so the quality preset does not buy lights; fewer
Shadowed lights, Thin objects off, a lower resolution or a larger Work budget do (the Director help says so). Below the floors (a tiny budget, e.g. 4K K8 with 20 M) the main
view shows no soft shadows at all rather than the previous 8 lights x 1 ray x 1 step; a budget sweep (every preset x 720p..4K x K1-8 x Thin x 1..1500 M, C++ test 34) proves the
admitted count never falls as the budget grows.

### 4. Gates (new `relight_grain_check.py`, `relight_cost_check.py`, `relight_scene_fixture.py`, `relight_horizon_study.py`, `relight_orbit_check.py`)

* **Planner (round 2: Thin hybrid, section 9.3; rows carry the lead count and the regular march, and Thin ON admits at least what OFF admits):** every production row equals an independent ladder; rays >= floor or one light at the floors cannot fit; A independent of the visible set; Thin near = 48 and far >=
  `min(minFar, 8)`; frame and draw ceilings; monotone admission over the whole budget slider; the main view admits 0 below the floors while a feed still degrades. Mutations rejected:
  rays-first order, A from the visible set, a planner that thins the Thin near segment (the plan it returns is rejected by the predicate every production plan passes), the uncapped
  Thin fallback (drops lights as the budget grows). C++ test 34 re-checks this with a verbatim copy of the pre-P0 planner (it breaks the floor in 52 rows).
* **Oracles (independent of the shader filter):** analytic disk-light profile by dense quadrature at a converged march: edge within **1 px**, 10-90 percent width within 20 percent,
  umbra <= 3/255, lit >= 252/255, full and half resolution at 1080p and 4K. Mutations rejected: a receiver bias pointing into the surface (lit minimum 0), a light 10 cm off (edge 31 px),
  an unconverged 64-step march (edge 4 px).
* **Noise:** error against a 64-ray reference at each plan's own march, all <= 8/255 and Medium 1080p K4 <= 6/255, plus the spread; pre-P0 must fail 4K Cinematic (24.8) and never be further from the
  reference than P0 by more than 0.5. Receivers: flat, normal-mapped, grazing 87 degrees (pre-P0 3.2x worse); lit speckle below 250/255 = 0.
* **Spectrum (plan O6; round 2 adds the shared-structure test, section 9.2; round 3 replaces its per-row fit by one global 2-D fit and adds the y, diagonal and row-parity mutants, section 10.2):** the penumbra patch minus its expected value (mean of the other seeds, which carries the ramp and the march bias) is bounded at **every non-DC bin by 2/255**
  (R2 Low 1.76, Medium 1.09; bands f < .04 / .04-.08 / .08-.15 / >= .15 cycle per pixel: 1.68 / 1.76 / 1.25 / 0.11) and the line/noise ratio <= 11 (3.3 / 3.0). Mutations rejected: the 4x4
  interleave (8.8 and 4.2, ratio 67), the x+y-degenerate rank of the first version (5.0 and 2.6, ratio 13), a 3/255 stripe at 0.094 cycle/px built into the production filter (section 9.2; the round-1 injection added the pattern after the detrend and tested only the threshold). Measured fact behind the
  block-wise shift: lattice constants alone cannot do it (a 90-degree-turned start lattice, a blue-noise tile of 32x32, tiles of 6-16 cells all measured; only random block shifts took Low under 2/255).
* **Hand-over (`relight_rank_check.py`):** the original bounds are restored (shared-blocker leak <= 1/255 at the default radius and any native range, <= 3/255 elsewhere, the 0.4 m range case). New
  fixture: a real 1080p perspective floor at eye 1.7 m through the production filter at radius 2 with native clip pairs 27/64, 54/128, 110/256 m: 0.00/255 leak past the native ramp; the filter
  without the hand-over stop leaks 16, 36 and 66/255 (mutation rejected). Round 2: the stop is per channel; on the same floor an unmapped channel beside a mapped one is byte-identical with and without the native clip, its grain in the band equals the stop-free render, and the scalar-stop mutant raises it by 1.5-1.7x (rejected).
* **Cost (`relight_cost_check.py`, GL timestamp queries, the production gather + filter of each version at its own effective plan, best of alternating batches, more batches while above the
  target):** reported against 1.15, fails above 2.5 x HEAD (user decision 10/22); see sections 6 and 9.
* **Orbit sweep (moving camera):** 192 camera poses; determinism, negative controls fire (95/94/143 of 151), the P0 pipeline is no worse than pre-P0 (worst 12 px for both; mean 3.41 both; 82 poses within 1 px both).

### 5. Existing tests that pinned the old planner or sampler (re-pinned, not weakened)

* Math tests 14, 15, 18, 22, 24, 25: the default-budget tables, the "reduce rays first" order and the K7/K8 per-light literals now state the ladder results. The sub-floor feed cases are unchanged.
  Test 34 and the sweep are new.
* `relight_vcam_check.py`: identity with the pre-P0 header where the ladder does not apply; where the main view admits nothing the old planner would have drawn, the test asserts admitted 0 / iterations 0.
* `relight_thin_check.py`: a full-resolution request may be planned at half for any preset (ladder rung 1); the baselines adopt only the P0 noise block, so byte comparisons still isolate the march.
* `relight_contact_check.py` / `relight_vcam_dispatch_check.py`: adopt the noise block's new anchor; the dispatch fixture now also pins the filter's native clip/offset uploads and the dropout uniform, each with a dropped-upload mutation.
* `relight_rank_check.py`: **no threshold is relaxed any more** (round 0 had accommodated the radius-8 corner at 14-16/255 and the 0.4 m case at 4/255; the hand-over stop removes the leak and the original 3/255, 1/255
  and 0.4 m bounds hold).
* `director_kit_gen.py` row pin 2162 -> 2163 (+1 Coverage readout), with a comment.

### 6. Cost

Plan target: gather + filter <= 1.15 x HEAD. **User decision 10/22** ("i can accept a cost penalty, so long as it works"): `relight_cost_check.py` REPORTS the ratio against that target and fails only above a hard ceiling of **2.5 x HEAD**; the +50 % rays control must read at least 1.05 x the plan's own ratio (relative; the old "must exceed the limit" line is dropped); the frame and draw budget assertions stay. Measured with GL timestamps, one query per draw, whole frames, the effective plan of each version, alternating batches, minima, RTX 5090 (the viewer, AlchemyTest.exe, was NOT running during the round-2 runs):

| Config | HEAD plan | P0 plan | HEAD ms | P0 ms | Ratio |
|---|---|---|---|---|---|
| Medium 1080p K4 | 8 rays x 12 steps half, r=1 | same plan, r=2 | 0.838 | 0.920 | **1.098** (gather 1.008x; filter 1.69x of a small part: 0.079 -> 0.133 ms) |
| Cinematic 4K K8 | 1 ray x 22 steps full, r=4 | 5 rays x 16 steps half, r=2 | 4.053 | 5.336 | **1.317** (gather 1.485x: 5 rays at a quarter of the pixels against one full-resolution ray; filter 0.80x) |

The round-1 note that read 0.73-1.02 at 4K Cinematic K8 was wrong: its HEAD baseline was a noisy 5.4-7.5 ms (the one-ray HEAD plan varies run to run), against 4.05-4.12 ms in every round-2 run. Codex measured 1.308 x and 1.300 x with the viewer running on the same GPU; this round reproduces it (1.297 x and 1.317 x in two runs). Medium 1080p K4 is inside the 1.15 target (1.10 x); 4K Cinematic K8 exceeds it by the accepted margin and is far below the 2.5 x ceiling. The control (+50 % rays) reads 1.64 x and 1.96 x HEAD, 1.5 x and 1.5 x the plans' own ratios. Before fix round 1 the same Medium configuration read 1.33-1.37 x (P0.5, deferred); the Thin plans are measured in section 10.8.

### 7. Not measured

In-world appearance, the viewer image identity, a HEAD-failing fixture for P0.5, and the cost at configurations other than the five of section 10.8 (other K). Round 2: the hybrid and the group fade are verified through the production dispatch in the gate harness, not in the viewer; the per-channel hand-over stop and the unmapped-band grain only on synthetic floors.

### 8. Fix round 1 (10/22) - what the review findings changed

| # | Finding | Result |
|---|---|---|
| 1 | Hand-over leak through the bilateral filter (14-16/255 at clip (4,5); 7-29/255 on a real grazing floor) | One-sided native stop in the filter (same window as the gather, uniforms uploaded for every filter pass), original bounds restored, Opus's grazing-floor fixture added (fails without the stop: 16/36/66, passes with it: 0.00). |
| 2 | GPU cost 1.33-1.37x | P0.5 deferred; 1.07-1.10x at Medium 1080p K4 (4K Cinematic K8 0.73-1.02x); cost gate added. |
| 3 | Grain gate accuracy relaxed (9/17 px edge, noise around own mean) | Edge oracle at a converged march with the 1 px bound; error against a 64-ray reference at each plan's march; the no-shift test uses 80 rows (the pre-P0 mean edge was itself noisy at 16). |
| 4 | Spectrum gate blind below 0.15; x+y-degenerate rank | Detrended, every non-DC bin <= 2/255, ratio kept; start lattice turned 90 degrees and block-wise shifts; mutations incl. the degenerate rank and an injected 0.094-cycle pattern. |
| 5 | Planner sub-floor fallback, 67-133 M cliff | Main view admits 0 below the floors; admission monotone in the budget (sweep test); Thin fallback capped. |
| 6 | Light pops at the coverage boundary | `admissionStrength` fade on the last admitted channel, tested incl. the unfaded mutation. |
| 7 | Coverage row stale | Cleared on entry, planned and rendered counts published, bound with the page, wording for 0. |
| 8 | P0.5 without a discriminating fixture | Deferred (reverted). |
| 9 | 3D harness hard-coded radius 1 | `relight_3d_plan.py` returns the radius of the tree's own policy (HEAD 1, P0 2 at 1080p half), `relight_3d_gl.py` uses it. |
| P2 | test 34 tautology; help texts; edge-on fallback; creases; radius jump | Real mutation of the near rule; Coverage, Thin objects, Work budget and Denoise help rewritten (kit regenerated, settings comments too); planarity test with agreeing one-sided differences replaces the edge-on fallback; per-tap tolerance (crease limit measured and documented, not cut below ~27 degrees); the radius step documented. |

### 9. Fix round 2 (10/22) - what the second round of reviews changed

Reviews: Codex (P0 x3, P1), Opus (P1 x4, P2), GLM, DeepSeek. Four user decisions were made in chat on 10/22 and are authoritative; where one of them changes a gate, the plan's original wording is kept next to it.

#### 9.1 User decisions

| Decision | Applied as |
|---|---|
| **U-cost**: "i can accept a cost penalty, so long as it works" | `relight_cost_check.py` REPORTS the gather + filter ratio against the plan's 1.15 target and FAILS only above a hard ceiling of **2.5 x HEAD**. The frame and draw budget assertions (planner gate: `iterations <= 1500 M`, one draw `<= 750 M`) are unchanged. The sensitivity control stays but is relative: the +50 % rays plan must read **at least 1.05 x the plan's own ratio**; the old "must exceed the limit" assertion is gone. |
| **U-accuracy**: Phase 0 is judged on GRAIN | The grain gate is unchanged: error against a 64-ray reference at each plan's OWN steps (<= 6/255 at Medium 1080p K4, <= 8/255 elsewhere). New **report-only** line per effective plan: the error against the TRUE 64-ray x 64-step reference. The coarse-step march bias it shows is left to the world-space phase. **Gate redefined by user decision 10/22**; the plan's original wording (64 rays / 64 steps within the same bounds) is kept in the log line, not enforced. |
| **U-thin**: hybrid | Thin objects ON never admits fewer lights than Thin OFF. When the budget cannot cover every admitted light at Thin quality, the top lights by importance get the Thin march and the others the regular march (section 9.3). |
| **U-fade**: smooth group fade | Every admitted light fades with `smoothstep(1, 1.05, score[i] / score[first dropped])`, per channel (section 9.4). The window was first 1.25; narrowed to 1.05 on 10/22 by the lead (a cluster of near-equal lamps faded almost out). |

#### 9.2 Findings and results

| # | Finding | Result |
|---|---|---|
| P0 1 | Spectrum gate tautology (Codex P0, Opus P1): the detrend subtracted the other seeds' mean, which cancels any screen pattern shared by every light UUID (a fixed 3/255 stripe at 0.094 cycles/px in the production filter passed) | A second spectrum test, **shared structure**: the mean of the rendered images over the seeds minus an INDEPENDENT expected profile, the analytic disk-light profile of the geometry (never a render) fitted per row by a blur, a scale, a shift, a gain and an offset (7 numbers, none of which can follow a narrow stripe), at a converged march so the plan's march bias is not mistaken for noise structure. Every non-DC bin <= 2/255. The mutant is a **production-filter shader variant** that adds the 3/255 stripe to the rendered full-resolution image, measured through the same chain: it must fail (reads 3.0/255). The seed-mean test alone still reads 1.75 on it: blind to it, as the reviews said. The honest spectrum stays under 2/255 without touching the noise: Low 1.84/255 (the seed-mean test: 1.76), Medium 0.64/255 (1.09). The old after-the-fact `pattern` injection is gone. |
| P1 2 | The native stop was one scalar depth mask for all four channels (GLM, DeepSeek, Opus: 1.3-1.6x grain, up to 7.2/255, in a 2-24 row band for a light with no native map) | `areaShadowFilterF.glsl`: the weight is a `vec4` and the stop is `mix(1, stop, clamp(area_contact_fade, 0, 1))` per channel (`area_contact_fade` is uploaded to the filter next to the clip and offset); `sum` and `weight` are accumulated per channel. Fully off for a channel whose fade is 0. New fixture on the grazing floor (eye 1.7 m, clips 27/64, 54/128, 110/256): a mapped channel keeps its 0.00/255 leak beside an unmapped one; the unmapped channel is **byte-identical with and without the native clip** and its grain in the band equals the stop-free render (rms of the cut pixels 4.07, 3.88 and 4.27/255 for the three clips, against 6.28, 6.15 and 7.19 with the scalar stop); the scalar-stop mutant raises it by 1.5-1.7x (rejected). |
| P2 3 | The all-taps-rejected fallback `textureLod(area_source, uv)` bypassed the stop | Per channel. The separable passes always keep their centre tap (weight 1), so the fallback is the centre texel and is unreachable; the upsample falls back to the bilinear tap with the lowest hand-over weight (the most traced one), never a mix. |
| P2 4 | Test 34: `mutatedVisible` never asserted; the oracle mirrors the planner's constants | `ensure("visible-set mutation is caught", mutatedVisible > 0)` is asserted (the line was already in the file when this round started and is kept); literal floor rows (4 rays, 8 far steps, 48 near steps, written as numbers) added for 4K K8 and 1080p K8, every preset. |
| P2 5 | `lights == 0` path admitted >= 1 with `channels == 0` | `finish()` sets `admitted = channels ? admitted : 0` (and `lead`); tested for the uniform and the Thin plan. |
| P1 | Codex 4: fading only the last admitted channel pops between retained lights (A = 3, scores [2, 1.100001, 1.099999, 1]: a 0.65 jump on a swap of the middle two) | Smooth group fade, section 9.4. |
| P1 | Opus 1, the Thin fallback cap loses lights for nothing | Replaced by the hybrid, section 9.3 (`segmentedFirstAdmission` and the cap are gone). |
| P1 | Codex 1/2 (accuracy against 64 x 64, cost 1.30 x at 4K Cinematic K8) | User decisions U-accuracy and U-cost. Codex's 1.30 x is reproduced below. |

#### 9.3 Thin objects hybrid (`hybridWithin`, `hybridPlan` in `alareashadowmath.h`)

* A (admitted lights) = **max(A_thin, A_off)**: A_thin is the pure Thin plan's admission (0 when it does not fit inside budget - feed reservation) and A_off the Thin-OFF plan's at the same budget. Both are monotone in the budget, so A is too; Thin ON never admits fewer lights than OFF.
* If the pure Thin plan draws and admits at least what OFF admits, it is the plan (every admitted light marches Thin). Otherwise A = A_off and the **largest nT** such that `nT` Thin lights and `A - nT` regular lights fit together is searched (nT = A down to 1; nT = 0 is exactly the Thin-OFF plan). The two groups share one resolution (they are drawn into the same targets) and degrade together on the ladder: rays to 8; the Thin far steps to half and the regular steps to their floor; rays to 4; the Thin far steps to their floor. **Superseded in round 3 (section 10.3): the regular group is the Thin-OFF plan, never degraded; only the Thin lights are squeezed.**
* Cost: a hybrid with Thin lights stays inside budget - feed reservation, which keeps the feed guarantee (every feed draws wherever the OFF feed draws, test 25). The Thin-OFF plan, nT = 0, is unchanged. A feed pass uses the same planner (`feedWorkload` -> `hybridWithin`, `floors = false`). (Round 3 removed the exception that stood here: a Thin main that spent more than the OFF main left its feeds less and cost them lights; there is no feed exception, section 10.1.)
* `Workload` carries `lead` (the top `lead` lights march `q`), `rest` and `restChannels` (the others march the regular way). Every other plan has `lead == admitted`.
* **Pipeline**: the planes are separate draws and so are the groups. Per plane the gather loops over the group ranges, sets `area_samples`, `area_steps`, `area_thin_objects` and `area_near_steps` per group (runtime uniforms, no shader reload), and splits draws at each group's own per-draw channel cap; the colour mask writes only the group's channels. A plan that is not a hybrid draws exactly as before.
* **Coverage row** now reads, e.g., "Soft shadows on 8 of 8 lights (2 thin-quality)" (section 9.5).
* Gates: C++ test 34 sweeps every preset x 720p..4K x K in {1,3,4,8} x feed reservation 0/8 x visible-set size x budget 1..1500 M (384 000 rows, 77 688 of them hybrids) and asserts both invariants (Thin ON admits at least what OFF admits; admission is monotone in the budget for both), the floors of both groups, the planning limit and per-draw caps of every hybrid, the cost recomputed from the target size. Mutations rejected: the pure Thin plan with a uniform fallback only where Thin does not fit (79 752 rows below OFF) and the capped fallback of round 1 (3 184 rows below OFF). `relight_grain_check.py` re-derives all 512 default-budget rows (now with `lead` and the regular march) by an independent enumeration of the ladder states, asserts Thin ON >= OFF for every height, K, preset and feed reservation, and rejects the no-hybrid mutant. `relight_vcam_dispatch_check.py` runs the production dispatch at a budget that makes a hybrid (Thin ON, 400 M, 1280x720): two marches in separate draws, Thin lights first and lowest indices, every channel drawn once, floors held, near segment only on Thin lights; four more dispatch mutations (rest march ignored, lead march ignored, ...) are rejected.

#### 9.4 Smooth group fade

`ALAreaShadow::admissionStrength(score, firstDropped)` = `smoothstep(1, 1.05, score / firstDropped)` (`kAdmissionWindow = 0.05`), 1 when `firstDropped <= 0` (and when nothing is dropped). The reference is an order statistic (the first dropped light's score), so a rank swap WITHIN the admitted set changes no strength: Codex's case (A = 3, [2, 1.100001, 1.099999, 1]) now gives strengths 1, 1, 1 on either order, and the same swap 3 percent above the threshold ([2, 1.0300001, 1.0299999, 1], strength 0.648) moves no strength by more than 1e-3 (the test bound; measured about 6e-5: the narrower window is 5x steeper, maximum slope 30 per unit of ratio, so the old 1e-4 bound became 1e-3 for a 2e-6 score change); the last-channel-only rule jumps by 0.35 there (mutation rejected). Eight lamps 3 percent apart with budget for three: the top two keep full strength, only the third (within 5 percent of the threshold) fades to 0.648. Frame-pure: this frame's scores only. Uploaded per channel as `area_dropout = 1 - strength` in the plane and channel order actually drawn; a plane that fails allocation fades only the channels that are drawn. A light with strength below 1/255 is not counted as rendered in the Coverage row (`admissionVisible`). The dispatch fixture pins the per-channel upload (and rejects a last-channel-only upload).

#### 9.5 Effective coverage with Thin objects (round 2; superseded by the table in section 3 and section 10.3. Default 1500 M budget, VCam feeds OFF; "T" = lights that march Thin objects quality, the rest march the regular plan)

| Screen, K | Thin OFF (all presets) | Thin ON (all presets) |
|---|---|---|
| 1080p K4 | 4 of 4 | 4 of 4 (4 thin-quality) |
| 1080p K8 | 8 of 8 | 8 of 8 (8 thin-quality; Low 4 rays x 80, others 5 rays x 64) |
| 4K K4 | 4 of 4 | **4 of 4 (3 thin-quality)**, Thin 4 rays x 57, the fourth light regular 4 x 8 (was 3 of 4) |
| 4K K8 | 8 of 8 | **8 of 8 (2 thin-quality)**, Thin 4 rays x 64-66, six regular 4 x 8 (was 3 of 8) |

At 4K the lights beyond the Thin ones keep the regular Low march (4 rays x 8 steps) because the budget cannot give every light 48 near steps; at 1080p everything fits Thin. The full 512-row grid is in `final_grain.log`.

#### 9.6 Numbers

**Cost** (section 6): Medium 1080p K4 **1.098 x** HEAD (0.920 vs 0.838 ms), Cinematic 4K K8 **1.317 x** (5.336 vs 4.053 ms), target 1.15 reported, ceiling 2.5; the control reads 1.64 x and 1.96 x.

**Grain** (RMS over 8 light UUIDs of the final filtered image against a 64-ray reference at the plan's own march, /255; limit 8, Medium 1080p K4 limit 6). The gate is unchanged by this round; only the plans are new (Thin ON at 4K is now a hybrid), so the table is round 1 -> round 2:

| Case | Round 1 error (spread) | Round 2 error (spread) |
|---|---|---|
| 4K Cinematic K8 / Medium K4 / Low (Thin OFF) | 3.8 (3.7) / 3.1 (3.1) / 5.9 (5.9) | 3.80 (3.69) / 3.11 (3.10) / 5.90 (5.85) |
| 1080p Low / Medium K4 / Thin Medium K8 | 5.8 / 3.1 / 4.2 | 5.75 (5.69) / 3.12 (3.08) / 4.23 (3.05) |
| 4K Thin K8: the Thin lights | 6.3 (3.5), 3 lights 4 x 56 | **5.31 (3.77)** 4 x 64 (Low 4.65 (3.28) 4 x 66); the six regular lights 4 x 8: 5.90 (5.85) |
| 4K Thin K4: the Thin lights | 6.3 (3.5), 3 lights 4 x 56 | **6.76 (3.89)** 4 x 57; the fourth, regular light 4 x 8: 5.90 |
| Grazing floor (~87 degrees) | 2.3 | 2.34 |

Worst: **6.76/255** (the Thin lights at 4K K4, 4 rays x 57 steps), round 1: 6.32. Medium 1080p K4: 3.12. Pre-P0 still fails 12 configurations (4K Cinematic 24.8).

**Spectrum** (every non-DC bin, limit 2/255): seed-mean detrend Low 1.76, Medium 1.09 (line ratio 3.3 and 3.0, limit 11); **shared structure against the analytic profile (independent detrend): Low 1.84 (band f < .04; .04-.08 0.71; .08-.15 0.26; >= .15 0.18), Medium 0.64** at a fit of blur 2.0 px, scale 1.000, shift -0.5 px. The production-filter stripe mutant (3/255 at 0.094 cycle/px in the rendered output) reads **3.00 and 2.98** and fails; 4x4 interleave 8.77 / 4.23 and the x+y-degenerate rank 4.99 / 2.64 still fail.

**Error against the true 64 x 64 reference (REPORT ONLY, not gated; gate redefined by user decision 10/22)**, so the march bias stays visible: 1080p Low (4 x 8) **36.81**, Medium K4 (8 x 12) **26.26**, High (12 x 20) 17.93, Cinematic K8 (11 x 32) 11.93, Cinematic K4 (22 x 32) 11.71; Thin lights 1080p 3.5-9.3 (4 x 80: 4.44, 8 x 80: 3.64, 5 x 64: 9.29); 4K Low/Medium K4 (4-8 x 8-12) 36.50 / 26.34, 4K 8 x 11 **51.02** (the worst), Cinematic K8 (5 x 16) 34.78, Cinematic K4 (8 x 22) 9.97, High K4 (9 x 20) 18.24, 4K Thin lights 4 x 57: 24.35, 4 x 64: 9.38, 4 x 66: 6.60. Codex's numbers (26/255 at Medium, 37 at Low) are reproduced exactly. Nearly all of it is bias (`mean offset` in the log): the march is too coarse, not noisy. The world-space phase is where it is fixed.

**Group fade window (lead decision 10/22):** with the first 25 percent window a cluster of near-equal lamps with some dropped faded almost out (the dispatch fixture's scores 8 - 0.02 i gave strengths 0.0003-0.0027 at A = 3). The window is now 5 percent: lights more than 5 percent ahead of the first dropped light are untouched, only those right at the threshold fade, so a cluster is slightly lighter overall (the dispatch fixture's near-tied lights, all within 1 percent, still fade: 0.007-0.06).

#### 9.7 Gates of round 2 (historical; the current gates are section 12.9; logs in `.tmp/ws_p0_r3/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full) | **PASS** (715 s) | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` (includes `relight_vcam_check` and `relight_vcam_dispatch_check` with the hybrid dispatch and 4 new mutants, `relight_contact_check`, orbit, cost, grain, thin, rank) | **PASS** (715 s) | `final_area.log` |
| `relight_thin_check.py`; `relight_off_check.py`; `relight_rank_check.py` | **PASS**; **PASS**; **PASS** (unmapped-band fixture added) | `final_thin.log`, `final_off.log`, `final_rank.log` |
| `relight_grain_check.py`; `relight_orbit_check.py`; orbit vs pre-P0 shaders; `relight_cost_check.py`; `relight_vcam_check.py` | **PASS** x5 | `final_grain.log`, `final_orbit.log`, `final_orbit_head.log`, `final_cost.log`, `final_vcam.log` |
| `relight_contact_check.py`, `relight_vcam_dispatch_check.py` | run inside the area and shader checks above (no stand-alone entry point) | `final_area.log` |
| Standalone `alrelightmath` tests | **PASS 34/34** | `math.log`, `math_compile.log`, `math_link.log` |
| `zs_check` per touched .cpp (pipeline, llfloaterdirector, alrelightmath_test) | **PASS** x3 | `zs_*.log` |
| Three generators `--check` (kit regenerated first, then page sources and manifest) | **PASS** x3 | `director_*_check.log` |
| `xui_layout_check.py --changed HEAD` | 0 findings (43 files) | `xui_layout.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **all gates OK except the compile gate**, which stops only at `tests/alframelimiter_test.cpp` (another session's unfinished frame-limiter work: `FramePacer::beginFrame/waitOnce` missing); this change's three TUs (pipeline, llfloaterdirector, alrelightmath_test) PASS in it; kit gen, kit lint, parity, XUI selftest, page sources, themes OK | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Notes: line endings. The files changed by this round are LF in the working tree like the index (an earlier tool had left some of them CRLF; git normalises both). Other sessions edit unrelated files of this tree at the same time (`alframelimiter.*`, `alderenderlist.*`, `llviewerdisplay.cpp`, menu XML, ...); none of them is touched by this change.

### 10. Fix round 3 (10/22) - what the third round of reviews changed

Reviews: Codex (P0 x2, P2), Opus (P0, P1 x3, P2), GLM (P1, P2), DeepSeek (P1, P2). Logs and scripts: `.tmp/ws_p0_r4/`. The user decisions of section 9.1 stand.

| # | Finding | Result |
|---|---|---|
| P0 1 | Thin ON removed VCam feed shadows (Codex: 720p Low K3 400 M, 8 feed slots, 1024 x 1024 feed: OFF 3 feed lights, ON 2; Opus: 1080p Medium K4 1500 M: 4 of 4 against 2 of 4; 70 of 512 default combinations) | Section 10.1: while feed slots exist a Thin or hybrid main is capped at the Thin-OFF main's cost and the feeds get EXACTLY the OFF allowance. 0 of 512 combinations lose a light at 1500, 1000, 600 and 300 M. The section 9.3 "feed exception" text is gone. |
| P0 2 | Spectrum gate blind to horizontal structure (per-row mean and per-row fit cancelled y-stripes, a multiplicative y-stripe, +3/255 on every 8th row) | Section 10.2: one global fit of the geometry-derived 2-D expectation plus a quadratic drift in y; seven new production-filter shader mutants, all rejected. The honest spectrum is unchanged (Low 1.85, Medium 0.65 /255): the noise needed no change and no limit was loosened. |
| P1 3 | The hybrid's regular group was grainier than Thin OFF (4K K4 Cinematic's fourth light 8 x 22 -> 4 x 8, grain 3.5 -> 5.9) | Section 10.3: the regular group is EXACTLY the Thin-OFF plan; the Thin lights get the leftover (possibly none). Asserted in every row of the C++ sweep (384 000 rows) and of the independent grid. |
| P1 4 | Thin/regular boundary pop under an in-set swap | Section 10.4: each Thin light marches the share `1 - smoothstep(1, 1.05, s_i / s_firstRegular)` of its rays the regular way. Exact at both ends, one ray per step in between, GPU-tested. |
| P1 5 | Tied lamps vanish: identical nearby lamps tie exactly and the group fade took the whole admitted cluster to 0 | Section 10.5 (LEAD DECISION on the user's behalf): an admitted light never fades below 0.5. |
| P1 6 | The upsample fallback took the lowest-weight tap even when its geometric weight was rejected (across the edge, or sky) | Only taps with a geometric weight above 0.05 count; otherwise the pixel's own texel. Thin-wall and object-against-sky fixtures; the previous fallback is rejected on both. |
| P2 | Coverage row, early returns, OOB, division guard, comments, cost control | Section 10.6. |

#### 10.1 Feeds: the Thin or hybrid main is capped at the Thin-OFF main (`hybridWithin(..., capAtRegular)`, `feedWorkload`)

> **Refined in round 4 (section 11.4).** The cap is no longer taken for eight assumed 2048 x 2048 feed slots: `capAtRegular` is gone, the main plan holds back what the feeds LIVE that frame can spend (`feedSpend`). With the old worst case (eight assumed 2048 x 2048 feeds) it is still exactly this cap; the feed allowance rule (exactly the Thin-OFF allowance, no feed exception) is unchanged.

* `regularCost(w)` = pixels x admitted x rays x steps of the regular main plan. `workloadOrUniform` passes `capAtRegular = feedSlots > 0`; `hybridWithin` then plans the pure Thin plan and every hybrid inside `min(budget - feedReservation, regularCost(OFF main))`. The OFF main is planned from the requested K, never the visible set, so the cap is frame-pure.
* `feedWorkload` computes the feeds' allowance from the Thin-OFF main plan whatever Thin objects is set to, so a feed gets exactly the allowance it gets with Thin OFF; the Thin main costs no more than the OFF main, so main plus eight feeds stay inside the one frame budget; and `hybridWithin` at an equal allowance admits at least what the OFF feed admits.
* Cost of the rule, measured: with feeds OFF 62 percent of the sweep's main rows keep a Thin march, with feeds ON 25 percent (the Thin lights have to fit under the OFF cost, so Low and Medium rarely do; High and Cinematic at 1080p keep it).
* Feed tables, Thin ON vs OFF (`feeds_r4.out`; main 720p..4K x feed 512 x 512, 1024 x 1024, 1080p, 2048 x 2048 x 4 presets x K1-8, 512 combinations per budget): feed lights ON < OFF in **0 of 512** at 1500, 1000, 600 and 300 M (round 2: 70, 66, 46, 30), the allowance identical in all 512. Codex's case: 47.2352 M per feed, ON 3 lights (1 x 60 Thin), OFF 3 (4 x 8) (round 2: 22.352 M, 2 lights). Opus's case (1080p Medium K4 1500 M, 1024 x 1024 feed): 162.617 M, ON 4 lights, OFF 4 (round 2: 21.6 M, 2 lights). 1080p Medium K8: 137.734 M, 8 and 8 (round 2: 2 of 8).
* Admission of a FEED is not monotone in the budget for Thin OFF either (below the floors a feed admits every light at one ray and one step, above them fewer at the floors; that sub-floor planner is unchanged by design, 1 884 steps in the sweep). The asserted invariants are: ON >= OFF at every budget, and ON has no admission drop that OFF does not have (0 extra). The main view's admission IS monotone for both (the 384 000-row sweep).
* Tests: math test 25 asserts the exact allowance, ON >= OFF admission, ON main <= OFF main, the frame total, Codex's and Opus's cases literally, and a 216 000-row feedWorkload sweep (3 mains x K {1,3,4,8} x 4 presets x 3 feed sizes x the whole slider); test 24 and 34 run with and without feed slots; the independent grid in `relight_grain_check.py` asserts the cost cap in all 512 rows.

#### 10.2 Spectrum gate (`relight_grain_check.py`, shared structure)

* The 2-D expectation is the analytic disk-light visible fraction for EVERY row of the patch (the receiver's own height against the light), never a render. ONE global fit (blur, scale, shift by the smallest residual; gain and offset by least squares) on the row-averaged image, applied to every row, then only a smooth quadratic drift in y (three numbers, at most 0.10/255 here) is removed. Nothing that varies along a row OR along a column can be absorbed.
* Limits: no bin above 2/255 (the plan's), and above 0.08 cycle/px none above 0.5/255 (new, tighter: the honest high bands read 0.26 and 0.19 at most; a +3/255 line on every 8th row reads 0.75).
* Honest values (unchanged noise): Low 1.85/255 (bands f < .04 1.85, .04-.08 0.69, .08-.15 0.26, >= .15 0.18), Medium 0.65 (0.65 / 0.45 / 0.19 / 0.18); drift 0.10 / 0.09.
* Production-filter shader mutants, injected into the rendered image before any measurement (peaks Low / Medium /255): x-stripe 0.094 c/px 3.01 / 2.99; **y-stripe 0.078 c/px 2.83 / 2.94; y-stripe 0.094 3.01 / 3.01; y-stripe 0.03 3.04 / 3.00; multiplicative y-stripe (8/255 of the value) 4.02 / 4.00; diagonal 0.094 2.36 / 2.43; row parity +-3/255 5.98 / 5.99; +3/255 on every 8th row 0.75 in the two upper bands**; all fail. (The round-2 gate read 1.81 / 0.66 on the 0.078 y-stripe.) The 4x4 interleave and the x+y-degenerate rank still fail (8.77 / 4.23, 4.99 / 2.64).
* A second scene with a horizontal edge was not added: the global 2-D fit already sees every y structure.

#### 10.3 Regular group = Thin-OFF plan (`hybridPlan`)

`hybridWithin` computes the OFF plan first; `hybridPlan(out, thin, off, ..., thinLights, planned)` keeps `rest = off.q` (rays, steps, resolution, per-draw channels) and gives `thinLights` Thin lights the leftover `planned - nR x px x rw`, degrading only the Thin march (rays to 8, far steps to half, rays to 4, far steps to their floor, never below the floors). At full resolution the Thin lights keep it only while 12 rays fit; otherwise that nT is dropped. The search takes the largest nT that fits; nT = 0 is exactly the Thin-OFF plan. DeepSeek's rung-3 ordering finding no longer applies (nothing is degraded jointly), and the division `drawCap / (px x rays x steps)` is guarded (`perRest == 0` returns false before any division).

Coverage with feeds OFF (1500 M): see the table in section 3. Cost of the rule: fewer Thin lights, never fewer or grainier regular ones. Grain: the regular lights read exactly the Thin-OFF values (4K K4 Cinematic 3.14 /255, as OFF; round 2: 5.90 for its fourth light); the Thin lights at 4K read 6.76 (4 x 57), 5.31 (4 x 64), 4.65 (4 x 66) /255, all under the limit 8 (Medium 1080p K4 3.12 <= 6).

#### 10.4 Thin/regular boundary (`thinWeight`, `area_regular_mix`, `area_rest_steps`)

> **Refined in round 4 (sections 11.1 and 11.3).** The Thin group now marches the regular ray count, and the regular share is `1 - max(0.5, smoothstep(...))` (so it is never above 0.5 and a tie keeps half of the rays Thin). The text below is the round-3 design.

* A Thin light i in a hybrid plan marches the share `m_i = 1 - smoothstep(1, 1.05, s_i / s_firstRegular)` of its rays the REGULAR way (the first regular light's score is the reference, an order statistic: swapping two Thin lights changes nothing). The rays are chosen by their stratum (`ray_u < m_i`, the ray's phase index and the pixel phase), so the share is exact in expectation and moves by one ray per 1/rays step; a regular ray uses the uniform offset and `min(area_rest_steps, area_steps)` steps. Cost: every ray costs at most a Thin ray (the regular count is clamped to the Thin count), so the budget holds with no reservation.
* At a rank swap between the last Thin light and the first regular light the Thin light is entirely regular (share 1); a clear leader (> 5 percent ahead) is all Thin (share 0). Uniforms are runtime (`area_regular_mix` vec4 per channel, `area_rest_steps`), default 0 = every other plan marches exactly as before.
* `screenSpaceRayUtil.glsl` gets a per-ray `thin` argument (the 10-argument entry point forwards `area_thin_objects`); `relight_rank_check.py` now asserts that undoing exactly those four edits gives HEAD's file byte for byte (it used to assert byte identity with HEAD), and the gather's housing lines in both march branches. `relight_thin_check.py` builds its pinned-older-utility mutants with the gather's single HEAD call (`head_trace_call`).
* Gates: C++ (`thinWeight` continuity under a swap, the unblended mutation pops by 1); GPU on the 4 cm rail (the regular march misses it, the Thin march catches it, 191/255 apart): share 1 is the regular light byte for byte, share 0 the pure Thin light, eight steps of 1/8 move at most 32/255 (one ray), steps of 1/64 at most 32; the all-rays-at-once mutant pops 191/255, the share-ignoring mutant breaks the identity; the dispatch fixture pins the per-channel upload of the share (from scores, last plane), the step count, and rejects four mutations (upload dropped, last channel only, rank instead of score, step count dropped).

#### 10.5 Tied lamps (lead decision on the user's behalf, 10/22)

Importance is constant within range / 2 of the camera, so identical nearby lamps tie EXACTLY, and `smoothstep(1, 1.05, 1)` is 0: with A < visible every admitted light of the cluster faded out ("No soft shadows right now"). `admittedStrength(score, next) = max(0.5, smoothstep(1, 1.05, score / next))` for an admitted light with importance (a light with score <= 0 stays gone; `admissionStrength` stays the raw term and drives `thinWeight`). This accepts at most a half-strength pop at an exact boundary swap and keeps the strength continuous inside the admitted set (an order statistic and a constant). Tests: eight identical lamps with budget for three keep 0.5, 0.5, 0.5 and count as rendered (the raw rule gave 0, 0, 0: mutation rejected); the 3-percent cluster is unchanged (1, 1, 0.648); the dispatch fixture's near-tied scores now upload dropouts in (0.45, 0.5] and reject the unfloored rule.

#### 10.6 P2 items

* Coverage: "(0 thin-quality)" when Thin was requested and fell back entirely (`mAreaShadowThinRequested`); `coverageText(..., thinRequested, lowMemory)` with an explicit `mAreaShadowLowMemory` published before the early return after planning, so an allocation failure reads "low video memory" and a budget plan of 0 reads "work budget". (wanted and planned were already published before allocation; the dead `planned == ~0u` default is kept for the pure tests.)
* The plane loop count is clamped by `selected.size` after the truncation (`count = plane x 4 < channels ? min(4, min(channels, size) - plane x 4) : 0`).
* `hybridPlan` divisions guarded; the header comments and the Work budget / Thin objects settings text no longer say the OFF plan is unchanged (P0.1 applies the resolution rung and the floors to uniform plans too, by design; the Work budget comment states the cap and the rule); kit regenerated, page sources and manifest regenerate unchanged.
* Cost control: `(3 s + 1) // 2` rays (8 -> 12, never the 13 of `int(1.5 s) + 1`), the actual counts printed; Thin ON and hybrid timing added (section 10.8).

#### 10.7 Grain

Worst P0 configuration unchanged at **6.76/255** (the 4K K4 Low Thin lights, 4 x 57), limit 8; Medium 1080p K4 3.12 (limit 6); worst noise 5.85. Round 2 -> round 3 where plans changed: 4K Cinematic K4 as Thin ON 3.14 (was 5.90 for its fourth light), 4K K8 Medium/High as Thin ON 3.22 (was 5.90 for six regular lights). Error against the true 64 x 64 reference (report only): worst 51.02 (4K, 8 x 11) as before.

#### 10.8 Cost (`relight_cost_check.py`, RTX 5090, viewer not running)

| Config | HEAD plan | P0 plan | HEAD ms | P0 ms | Ratio |
|---|---|---|---|---|---|
| Medium 1080p K4 | 8 x 12 half | same, r=2 | 0.845 | 0.919 | 1.087 |
| Cinematic 4K K8 | 1 x 22 full | 5 x 16 half | 4.070 | 5.349 | 1.314 |
| Thin Medium 1080p K4 (pure Thin) | 8 x 80 half | same | 3.940 | 4.175 | 1.060 |
| Thin Low 4K K4 (hybrid: 3 Thin 4 x 57 + 1 regular 4 x 8) | 2 x 80 half | | 4.265 | 4.832 | 1.133 |
| Thin Low 4K K8 (hybrid: 2 Thin 4 x 66 + 6 regular 4 x 8) | 1 x 80 half | | 4.713 | 5.489 | 1.165 |

Target 1.15 exceeded at Cinematic 4K K8 and the K8 hybrid, accepted (U-cost); ceiling 2.5. The +50 % control (12, 8, 12, 6 and 6 rays, i.e. (3 s + 1) // 2 of 8, 5, 8, 4, 4) reads 1.28-1.96 x its own ratio. Codex's separate measurement (hybrid about 1.14 x at 4K K4/K8, gather draws 2 -> 4) agrees.

#### 10.9 Gates of round 3 (historical; the current gates are section 12.9; logs in `.tmp/ws_p0_r4/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full; includes the area suite with the production dispatch, contact, orbit, cost, grain, thin and rank) | **PASS** (772 s) | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` | **PASS** (791 s) | `final_area.log` |
| `relight_thin_check.py`; `relight_off_check.py`; `relight_rank_check.py` | **PASS**; **PASS**; **PASS** | `final_thin.log`, `final_off.log`, `final_rank.log` |
| `relight_grain_check.py`; `relight_orbit_check.py`; orbit vs pre-P0 shaders; `relight_cost_check.py`; `relight_vcam_check.py` | **PASS** x5 | `final_grain.log`, `final_orbit.log`, `final_orbit_head.log`, `final_cost.log`, `final_vcam.log` |
| `relight_contact_check.py`, `relight_vcam_dispatch_check.py` | inside the area and shader checks above (no stand-alone entry point) | `final_area.log` |
| Standalone `alrelightmath` tests | **PASS 34/34** | `math.log`, `math_compile.log`, `math_link.log` |
| `zs_check` per touched .cpp (pipeline, llfloaterdirector, alrelightmath_test) | **PASS** x3 | `zs_*.log`, `zs_rc.txt` |
| Three generators `--check` (kit regenerated first: the source-bundle hash covers settings.xml) | **PASS** x3 | `director_*_check.log` |
| `xui_layout_check.py --changed HEAD` | 0 findings (43 files) | `xui_layout.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **all gates OK**, including the compile gate (`-Base 4dd9ff661c1 -IncludeTests`) | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Notes: the first full run of this round failed only on a syntax slip in a message string of `relight_rank_check.py` (fixed, rank, shader and area gates rerun; the others were unaffected). Other sessions edit unrelated files of this tree (`alframelimiter.*`, `alderenderlist.*`, `llviewerdisplay.cpp`, menu XML, ...); none of them is touched by this change.

### 11. Fix round 4 (10/23) - what the fourth round of reviews changed

Reviews: Codex (P0 x2), Opus (NO P0; P1 x1, P2 x5), GLM (P1 x2, P2 x2), DeepSeek (P0, P1 x2, P2 x3). Logs and scripts: `.tmp/ws_p0_r5/`. The user decisions of section 9.1 and the lead decisions of sections 10 and 11 stand. Lead decisions of this round (10/23): the hybrid's Thin group uses the regular ray count; the regular share of a Thin light is `1 - max(0.5, thinWeight)`; the feed reservation counts the feeds that are live; feed monotonicity is a pre-existing planner limit (section 11.7).

| # | Finding | Result |
|---|---|---|
| P0 1 | Boundary ray-count mismatch (Codex: 4K Medium K4, Thin 4 x 64 beside regular 8 x 12, endpoints 22/255 apart filtered; GLM, DeepSeek P1; Opus P2) | Section 11.1: the Thin group of a hybrid marches EXACTLY the regular (Thin-OFF) ray count; only the far steps differ. If that leaves no room for a Thin light the plan has fewer Thin lights, down to none. The boundary light at share 1 is the regular light's estimator byte for byte (raw) and within 0/255 after the production filter, on all 7 production hybrid plans of the grid. |
| P0 2 | Feed-cap quantity mismatch (DeepSeek: `regularCost(off)` vs `main.iterations`) | Section 11.2: they ARE the same quantity (the Thin-OFF main planned for the requested K, whatever the visible set); both now use `regularCost()`. The proposed cap at `off.iterations` is the one that would be wrong (visible-set dependent). New sweep: main + every live feed <= budget, with the cap of lights and with one visible light, 0 violations in 672 000 rows. |
| P1 3 | Tied lamps: `thinWeight` had no floor, so exact ties made every Thin light 100 percent regular while Coverage said "thin-quality" (Opus) | Section 11.3: regular share `1 - max(0.5, thinWeight)`; a light counts as thin-quality only while that share is below 0.5. |
| P1 4 | VCam ON makes Thin objects do nothing (Opus P2-4; the planner reserved for 8 slots x 2048 x 2048) | Section 11.4: the reservation is for the feeds LIVE that frame (count and render target sizes), current state only. |
| P2 5 | Spectrum drift unbounded; Hann scalloping lets a 2.6/255 half-bin diagonal read 1.88 (Opus) | Section 11.5: drift <= 0.5/255 asserted (reads 0.10 / 0.09); the peak and band reads are zero-padded 2x (the half-bin diagonal reads 2.48 and fails). |
| P2 6 | Upsample fallback: bilinear `textureLod`, chosen globally (DeepSeek) | Section 11.6: `texelFetch` everywhere, chosen per channel. |
| P1 7 | The filter program's `depthMap` may be unbound (GLM) | Section 11.6: bound; where, and a static pin with two mutations. |
| P2 8 | "low video memory" shown for a budget effect (GLM) | Section 11.6: only the explicit allocation-failure flag says it. |
| P2 9 | Documentation of the 0.5 floor and of the Custom step clamp (Opus) | Section 11.7. |
| LEAD OVERRULE | Feed admission is not monotone in the budget (Codex P0-2) | Section 11.7: pre-existing planner limit, deferred. |

#### 11.1 Boundary: the Thin group marches the regular ray count (`hybridPlan`, `areaShadowF.glsl`)

* `hybridPlan` starts the Thin lights at `off.q.samples` (the regular, Thin-OFF ray count, which is also the number of disk strata the shader loops over) and never changes it; the ladder is reduced to the far steps (down to the common floor of 8, near steps never thin). The previous Thin ray ladder (rays to 8, to 4) is gone, so DeepSeek's rung-3 ordering remark is moot. The largest number of Thin lights that fits is used, down to none (exactly the Thin-OFF plan).
* Effect on the default 1500 M grid (VCam feeds OFF): 4K K4 Medium is now the OFF plan (it was 2 Thin lights at 4 x 64 beside 2 regular 8 x 12; the sections 3 and 10 tables are corrected); every other row is unchanged (Low keeps its 3 + 1 and 2 + 6 hybrids at 4 rays). The hybrid sweep of 384 000 rows has 11 588 hybrids (18 944 before) and asserts `thin.samples == rest.samples` in every one; 1 940 of them would have a different Thin ray count under the previous ladder (the mutation, also rejected by a header mutant: lowering the Thin rays first fails tests 24, 34 and 35).
* GPU: `relight_grain_check.py` section 6b runs the boundary on the 7 distinct production hybrid plans of the grid (4 x 56 / 57 / 66 / 70 / 74 / 78 Thin beside 4 x 8, and 8 x 65 beside 8 x 12, all half resolution), not on a hand-made pair: share 1 equals the regular light byte for byte on the raw gather, **the production-filtered endpoints differ by 0/255 (limit 1)**, share 0 is the pure Thin light, and the raw and the filtered boundary move by at most one ray (raw 64 or 32/255, filtered 37 or 29/255) per 1/rays step. The previous round's plan (Thin 4 x 64 beside regular 8 x 12) on the same fixture differs by 6/255 filtered (22/255 in Codex's 4K scene), above the gate's threshold of 4, so it is a rejected mutation. Shader mutants unchanged: switching all rays at once pops 191/255, ignoring the share breaks the identity.
* The step clamp at the boundary (Opus P2-5): documented here in round 4 (a regular ray of a Thin light marched `min(area_rest_steps, area_steps)` steps, so a Custom uniform count above the Thin light's steps, 64 against 48 + 8, marched 56). **Removed in round 5 (section 12.4):** the shader now marches the regular step count unclamped (at most 64) and the planner charges the Thin light the larger of the two.

#### 11.2 Feed-cap quantity (DeepSeek P0), reconciled

`hybridWithin` used to cap at `regularCost(off)` = pixels x admitted x rays x steps, `feedWorkload` reserved `main.iterations`. DeepSeek read the second as `min(visible, admitted)` lights and saw 398 M against 99.5 M with two visible lights. That is not what is computed: `feedWorkload` plans its main with the REQUESTED K as the light count (`workload(uniform, ..., requestedLights, ..., requestedLights)`), so `min(K, admitted) == admitted` and both are the Thin-OFF main planned for K. To make this structural, `feedWorkload` now subtracts `regularCost(main)` (the same function the cap and the new `feedSpend` use), and the main-view `off` plan of `hybridWithin` is independent of how many lights are visible (test 35: the reserve `regularCost` is identical for 1, K/2 and K visible lights in all 672 000 rows, and no plan changes with the visible set). The suggested cap at `off.iterations` is the wrong one: `off.iterations` counts only the visible lights, so the Thin plan would depend on the visible set (frame-pure but not stable: the plan would change when a light leaves the frustum). Mutating the reservation to it is rejected by tests 34 and 35. Assertion over the sweep (test 35, 4 mains 720p..4K x K {1,3,4,8} x 4 presets x 7 feed sets x the whole 1..1500 M slider, 672 000 rows): `basisCost(main) + sum over live feeds of basisCost(feed) <= budget` with every feed planned for K lights and again with one visible light per feed (`basisCost` = every admitted light at its group's march, whatever the visible set), including mains with fewer visible lights than K. 0 violations. (Mutation: reserving nothing for the live feeds fails tests 14, 22, 23, 24, 25, 34 and 35.)

#### 11.3 Tied lamps and the Thin share (`regularShare`, `thinQuality`)

`thinWeight(score, firstRegular)` is now `admittedStrength` (floor 0.5, like the admission), `regularShare = 1 - thinWeight` (never above 0.5 for a light with importance; 0 when no regular light follows). The pipeline uploads the regular share per channel and counts a Thin light as thin-quality in the Coverage row only when `regularShare < 0.5` (`thinQuality`): a fully tied cluster reads "(0 thin-quality)". Test: four identical sconces (3 Thin + 1 regular) count 0 thin-quality; mutation without the floor is rejected (test 34); the dispatch fixture uploads `1 - max(0.5, ...)` per channel and plane, and rejects the unfloored rule.

#### 11.4 Feeds that are live (`FeedSet`, `feedSpend`, `LLPrismLens::liveAreaFeeds`)

**Round 5 supersedes two details of this section** (12.1 and 12.3): the extent of a live feed is its FIXED capacity target, not the per-frame render target named below (that one follows the presented FPS), and the set is read from the registry once per frame and a pass is admitted by feed identity. The feed table below uses the per-frame sizes of round 4 and is replaced by the table in 12.1.

* `FeedSet` = the live feeds' count and render target sizes (at most 8; a plain `unsigned` still means that many 2048 x 2048 feeds, the previous assumption, so every earlier test is unchanged). The pipeline builds it from the Prism registry: camera feeds prepared THIS frame at full quality (not preview-only, not the test pattern), `mFrame.mFrame == gFrameCount`, target size. Current state only; the main view and every feed pass of one frame read the same set, and a feed pass that is not in the set (a stale frame) is refused. `capAtRegular` is gone. The main plan's feed pass limit is the live count (`FeedPasses::admit(frame, live_feeds.count)`).
* Each live feed keeps EXACTLY its Thin-OFF allowance `(budget - regularCost(Thin-OFF main)) / live feeds` (unchanged rule, no feed exception). A feed cannot spend more than `feedSpendBound` = its target (at the request's resolution) x K x requested rays x requested steps, so the main plan holds back `feedSpend` = sum over live feeds of `min(allowance, bound)`, or the whole pool when every feed is bound by its allowance (then the main plan is capped at the Thin-OFF cost, exactly the round-3 rule). With no live feed the main view has the whole budget.
* **Documented consequence:** turning a camera feed ON can lower the main view's Thin quality (the reservation grows with each live feed up to the pool); it never lowers the number of lights the main view shadows (the regular group is the Thin-OFF plan) nor a feed's lights. Adding a feed can also raise it, because each feed's allowance shrinks with the count.
* Invariants re-run under the new reservation (test 35, 672 000 rows, the seven feed sets of the table below): Thin ON main admits >= Thin OFF and draws wherever OFF draws (0 violations), main admission is monotone in the budget (0 drops), the feed's allowance is exactly the Thin-OFF allowance, Thin ON feed admission >= OFF (0 rows), no feed admission drop that OFF does not have (0), the frame total above, the feed inside its own bound.
* Thin coverage, default 1500 M (admitted lights, Thin lights, Thin rays; "rest" is the regular group; `probe2.out`):

| Screen, K | Feeds | Low | Medium | High | Cinematic |
|---|---|---|---|---|---|
| 1080p K4 | no live feed | 4/4, all Thin 4r | 4/4, all Thin 8r | 4/4, all Thin 9r | 4/4, all Thin 9r |
| 1080p K4 | 1 x 1024x576 | 4/4, all Thin 4r | 4/4, all Thin 8r | 4/4, all Thin 7r | 4/4, all Thin 8r |
| 1080p K4 | 1 x 1280x720 | 4/4, all Thin 4r | 4/4, all Thin 6r | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 2 x 1280x720 | 4/4, all Thin 4r | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 1 x 1920x1080 | 4/4, all Thin 4r | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 2 x 1920x1080 | 4/4, 1 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 8 x 2048x2048 (old worst case) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K8 | no live feed | 8/8, all Thin 4r | 8/8, all Thin 5r | 8/8, all Thin 5r | 8/8, all Thin 5r |
| 1080p K8 | 1 x 1024x576 | 8/8, all Thin 4r | 8/8, 1 Thin 8r + rest 8x12 | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 1 x 1280x720 | 8/8, 7 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 2 x 1280x720 | 8/8, 1 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 1 x 1920x1080 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 2 x 1920x1080 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 8 x 2048x2048 (old worst case) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 2160p K4 | no live feed | 4/4, 3 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 1 x 1024x576 | 4/4, 2 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 1 x 1280x720 | 4/4, 2 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 2 x 1280x720 | 4/4, 1 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 1 x 1920x1080 | 4/4, 1 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 2 x 1920x1080 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 8 x 2048x2048 (old worst case) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K8 | no live feed | 8/8, 2 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 1 x 1024x576 | 8/8, 1 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 1 x 1280x720 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 2 x 1280x720 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 1 x 1920x1080 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 2 x 1920x1080 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 8 x 2048x2048 (old worst case) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |

Set against the previous planner (eight assumed 2048 x 2048 feeds, last row of each block, which is exactly the round-3 cap): at 1080p K4 a single 1024 x 576 or 1280 x 720 feed leaves Thin on every light for every preset, two 720p feeds or one 1080p feed take Medium back to the OFF plan (their allowance can use the whole pool), and at 4K only Low keeps any Thin light with a small feed (the OFF plan uses nearly the whole budget at 4K Medium and above). Rows counted over the whole sweep (Thin lights present): no live feed 55 698, one 1024 x 576 feed 49 472, one 720p feed 46 228, two 720p feeds 39 345, one 1080p feed 38 012, a mixed 1080p + 512 + 640 set 48 205, eight 2048 x 2048 24 042.

#### 11.5 Spectrum gate (`relight_grain_check.py`)

* The fitted quadratic y-drift is asserted <= 0.5/255 (new `SPECTRUM_DRIFT`); honest readings Low 0.10, Medium 0.09 (a 3/255 y-wave of up to one cycle per patch read 1.63 and a 6/255 y-ramp 0.00 before, and passed).
* The peak and band reads are taken on a grid zero-padded 2x in both axes (the line ratio keeps the unpadded bins its limits were calibrated on). The honest values did not move: Low 1.85/255 (limit 2.0, **margin 0.15**; bands f < .04 1.85, .04-.08 0.74, .08-.15 0.26, >= .15 0.18), Medium 0.66/255 (margin 1.34); seed-mean detrend R2 Low 1.77, Medium 1.11 (limit 2.0), line ratios 3.3 and 3.0 (limit 11). The new mutant, a 2.6/255 diagonal stripe whose frequency lies half-way between two bins of the patch in both axes, reads **2.48 / 2.49** (limit 2.0, margin 0.48) and fails; Opus measured 1.88 for it under the unpadded Hann read. All earlier mutants still fail: x-stripe 3.01 / 2.99, y-stripes 2.83-3.04, multiplicative y-stripe 4.02, diagonal 2.78 / 2.87, row parity 5.98, +3/255 on every 8th row 0.75 in the two upper bands (limit 0.5), interleave 8.77 / 4.23, x+y-degenerate rank 5.83 / 2.83.
* Opus's remaining notes: a +-3/255 8x8 block pattern reads 0.81 in the spectrum and every 16th row +3/255 reads 0.38. **This section wrongly said both were caught** (by the RMS gate and the 0.5 band limit): neither was (Opus, round 5: the 8x8 block moves the RMS 6.76 -> 7.40 under the limit of 8, and the comb stays under 0.5; 4x4 blocks and every 32nd row +6 were blind too). Round 5 adds a block-structure read and a row read that fail all of them (section 12.5).

#### 11.6 Upsample fallback, `depthMap`, Coverage label

* `areaShadowFilterF.glsl`: every fallback value is a `texelFetch` of ONE source texel (the pixel's own texel, and the chosen tap), and the choice is made PER CHANNEL: among the taps that were geometrically on the centre's surface a channel with a native map (contact fade > 0) takes the lowest hand-over weight, a channel without one the nearest tap (key `ns * fade - 0.001 * bilinear`, vec4). In the harness the filtered-vs-fetched own texel changes 0 strip pixels (the production filter inputs are bound `TFO_POINT`, so the two are equal there): this is robustness against the sampler state, not a measured fix. The per-channel choice is likewise equal to the global one wherever it can matter (a channel with fade < 1 keeps non-zero weights and never reaches the fallback); the thin-wall and object-against-sky fixtures and the previous-fallback mutant (no geometric gate) are unchanged and pass.
* `depthMap` in the filter (GLM P1): it IS bound. `pipeline.cpp:27272` draws the filter with `bindDeferredShader(gAreaShadowFilterProgram)`, which enables `LLShaderMgr::DEFERRED_DEPTH` (the reserved uniform `"depthMap"`, `llshadermgr.cpp:1526`) on the deferred target at `pipeline.cpp:25024-25030`, and the program is created deferred (`llviewershadermgr.cpp:3621-3626`, `isDeferred = true`). No code change; `relight_rank_check.py` now pins all four facts with two mutations (filter drawn without the deferred binder; binder without the depth bind).
* Coverage: `coverageText` says "low video memory" only for the explicit allocation-failure flag (`mAreaShadowLowMemory`). The old second clause (`rendered < min(wanted, planned)`) is gone: a plan that admits fewer lights, or a draw or plane cut by the budget, is a work-budget effect.

#### 11.7 Documentation items and the lead overrule

* The 0.5 floor pops by 0.5 at EVERY coverage-boundary crossing, not only at exact ties: when a light crosses from admitted to dropped (or back) its strength goes 0.5 <-> 0, and the Thin share of a light crossing between the last Thin light and the first regular light goes 0.5 <-> 0. What stays continuous is the strength WITHIN the admitted set (an order statistic and a constant), which is what the previous rounds' continuity tests cover. The floor is the lead's decision of 10/22 and 10/23 (tied lamps would otherwise vanish as a group).
* Not changed (noted): DeepSeek P1 that `lead` is a step function of the budget and of the target size (changing the Work budget or the resolution moves a light from Thin to regular in one frame; both are user actions, like a preset change); GLM's note that the resolution rung, the `max(area_params.z, 1.0)` noise cell and the 2 px filter floor change the Soft-shadows-ON image of the default plan (these are the P0.1 and P0.2 changes of sections 1 and 8; Soft shadows OFF stays byte-identical); DeepSeek P2 that the "planned" test cannot fail while `kAdmissionFloor` is above 1/255 (it is a pure-function property, kept for the case the floor changes).
* **Lead decision 10/23 (corrected in round 5): the budget-slider feed cliffs are a pre-existing planner limit, but the 55 M -> 56 M drop was NOT.** Codex P0-2 (720p main, Low, K3, eight slots, 512 x 512 feed, 55 M -> 56 M: the allowance rises 4.1102 M -> 4.2352 M and the admission fell 3 -> 2) was called pre-existing in round 4. That was wrong: HEAD keeps 3 lights (2 x 8) at both budgets, and the drop came from the light-dropping rung of the P0.1 ladder (rung 6) being applied to feed plans too (Codex round 5 reproduced it with HEAD's planner against all 512 cached rows). Round 5 applies rung 6 to the main view only (section 12.2); what remains of the old text are HEAD's reservation cliffs, listed precisely there. The assertions "Thin ON >= Thin OFF" and "ON never drops where OFF does not" stay.

#### 11.8 Grain and cost

Grain: worst P0 reference error **6.76/255** (the 4K K4 Low Thin lights, 4 x 57; limit 8), noise 5.85, Medium 1080p K4 3.12 (limit 6): unchanged, because only the plans the fix touches (4K K4 Medium, now the OFF plan, 3.14 /255 class) changed. Grazing floor 2.34 (pre-P0 7.59). Cost (RTX 5090, viewer not running, `final_cost.log`): Medium 1080p K4 1.088x HEAD (0.925 vs 0.850 ms); Cinematic 4K K8 1.335x (5.411 vs 4.054 ms); Thin Medium 1080p K4 (pure Thin) 1.057x (4.185 vs 3.961 ms); Thin Low 4K K4 hybrid (3 Thin 4 x 57 + 1 regular 4 x 8) 1.147x (4.795 vs 4.179 ms); Thin Low 4K K8 hybrid (2 Thin 4 x 66 + 6 regular 4 x 8) 1.169x (5.521 vs 4.723 ms); hard ceiling 2.5, user-accepted target 1.15 exceeded as before at Cinematic 4K K8 and the K8 hybrid. The +50 percent control (rays x 3/2 + 1/2) reads 1.54, 1.97, 1.55, 1.62 and 1.51 x its own ratio (limit 1.05). The boundary change moved no cost: 4K Medium K4 is the OFF plan now and is not in the table.

#### 11.9 Gates of round 4 (historical; the current gates are section 12.9; logs in `.tmp/ws_p0_r5/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full; includes the area suite with the production dispatch, contact, orbit, cost, grain, thin and rank) | **PASS** (796 s) | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** (12 s) | `final_shader_thin.log` |
| `relight_area_check.py` (production dispatch with 40+ mutations, contact) | **PASS** (814 s) | `final_area.log` |
| `relight_thin_check.py`; `relight_off_check.py`; `relight_rank_check.py` | **PASS**; **PASS**; **PASS** | `final_thin.log`, `final_off.log`, `final_rank.log` |
| `relight_grain_check.py`; `relight_orbit_check.py`; orbit vs pre-P0 shaders; `relight_cost_check.py`; `relight_vcam_check.py` | **PASS** x5 | `final_grain.log`, `final_orbit.log`, `final_orbit_head.log`, `final_cost.log`, `final_vcam.log` |
| `relight_contact_check.py`, `relight_vcam_dispatch_check.py` | inside the area and shader checks above (no stand-alone entry point) | `final_area.log`, `final_shaders.log` |
| Standalone `alrelightmath` tests | **PASS 35/35** (new test 35, 672 000 rows) | `math.log`, `math_compile.log`, `math_link.log` |
| Header mutations (`mutate_math.py`): feed spend ignored; cap at the visible-set cost; Thin rays lowered first; unfloored regular share; memory label from the heuristic | all five **rejected** (tests 14, 22-25, 34, 35) | `mutate_math.out` |
| `zs_check` per touched .cpp (pipeline, llprismlens, alrelightmath_test) | **PASS** x3 | `zs_*.log`, `zs_rc.txt` |
| Three generators `--check` | **PASS** x3 (the settings text change needed no regeneration) | `director_kit_gen_check.log`, `director_page_sources_check.log`, `director_functional_manifest_check.log` |
| `xui_layout_check.py --changed HEAD` | 0 findings (43 files) | `xui_layout.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **all gates OK** (it did not fail on the other session's `alframelimiter` files) | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Notes: the first full area run of this round failed on a dispatch fixture mutant (`regular_mix_not_per_channel`) that survived because the fixture only looked at the LAST plane's regular-share upload, which holds no Thin light in the fixture's hybrid (Medium needs 8 Thin rays now, so the lead is 2 of 8): the fixture now records every plane's upload, and all mutants are rejected. Edits the permission system denied: none. Other sessions edit unrelated files of this tree (`alframelimiter.*`, `alderenderlist.*`, `llviewerdisplay.cpp`, menu XML, `indra/llwindow/*`, ...); none is touched by this change.

### 12. Fix round 5 (10/23) - what the fifth round of reviews changed

Reviews: Codex (P0 x1), Opus (P0 x1, P2 x5), GLM (P1 x1, P2 x2), DeepSeek (P1 x2, P2 x3). Logs and scripts: `.tmp/ws_p0_r6/`. The user decisions of section 9.1 and the lead decisions of sections 10 and 11 stand. Math tests: 36/36 (new test 36, test 22 rewritten, test 35 unchanged); the pre-existing Python mirror of the planner (`relight_grain_check.py`) follows the header.

| # | Finding | Result |
|---|---|---|
| P0 1 | The main Thin plan depends on the presented FPS (Opus): a live feed was reserved at its PER-FRAME target, which `PrismAdaptivePerformance` scales with the frame rate | Section 12.1: each live feed is reserved at its FIXED capacity target (`liveFeedReservationExtent`); the main plan is identical across every adaptive scale and display size. |
| P0 2 | The 55 M -> 56 M feed-admission drop is introduced by Phase 0 (Codex): rung 6 (fewer lights) applied to feed plans | Section 12.2: rung 6 is main-view only; a feed keeps HEAD's fixed-K fallback. Admission is monotone in the feed allowance (1 536 384 rows, 0 drops); the budget-slider cliffs that remain are HEAD's reservation cliffs, listed. Section 11.7 corrected. |
| P1 3 | The live feed set is re-queried per pass; passes admitted by a bare count (GLM, DeepSeek) | Section 12.3: one snapshot per `gFrameCount`, passes admitted by feed identity. |
| P1 4 | Hybrid regular rays clamp their steps to the Thin draw's; the plan undercharges them (DeepSeek) | Section 12.4: unclamped (<= 64), charged `max(Thin steps, regular steps)` everywhere. |
| P2 5 | Spectrum gate blind to 8x8 / 4x4 blocks and every 16th / 32nd row; the drift assertion never exercised alone (Opus) | Section 12.5: block-structure and row reads; a y-ramp mutant that fails on the drift alone; section 11.5 corrected. |
| P2 6 | Upsample fallback own texel in a different base alignment than the bilinear neighbourhood (DeepSeek) | Section 12.6. |
| P2 7 | `coverageText` `planned` parameter dead (GLM) | Removed with its caller argument and `mAreaShadowPlanned`. |
| P2 8 | Doc: "live" includes offline sources; a display entering view changes the plan; pipeline comment (Opus) | Section 12.7. |

#### 12.1 The main plan does not follow the frame rate (`liveFeedReservationExtent`, `LLPrismLens::liveAreaFeeds`)

* **The defect.** The reservation sized each live feed by `mFrame.mTargetWidth/Height`, which is `bucketedTargetExtent(on-screen display size x prismAppliedResolutionScale())`; with `PrismAdaptivePerformance` on (the default) the scale is driven by `gFPSClamped`. At 1080p K8 Medium with one VCam display about 700 px wide, an FPS dip that takes the scale to 0.7 moved the feed from 1024 to 512 pixels, and the main view went from "1 Thin + 7 regular" to "8 Thin at 4 x 64": every light's ray count halved (a positive feedback loop: low FPS shrinks the feed, the main view spends the freed budget, FPS drops again). That broke the "no timing input" rule and the frame-purity condition of this plan.
* **The fix.** `liveAreaFeeds` now reports, for each live feed, the FIXED capacity target `LLPrismLens::liveFeedReservationExtent(outputCapacity(slot), aspect)` = `cameraTargetSize(cap, cap, aspect, cap)` (the largest target of the feed's aspect inside the slot's output capacity; capacity 1024: 1024 x 576 at 16:9; 2048: 2048 x 1152), plus the feed's registry slot. It depends on the capacity and the aspect only: not on the displays' on-screen size (so not on the dolly distance either), not on the adaptive resolution scale. The real target of a pass is never larger (asserted over a scale x display grid), so the reservation stays conservative. `outputCapacity` itself follows the user's projector resolution setting and the Gate's held output, never a timer.
* **Test (math test 36, part 4; `relight_rank_check.py`).** For three fixed sets of live slots (1024 / 1024 + 2048 / 1024 + 1024 + 2048, mixed aspects), every preset x main size (720p, 1080p, 4K) x K x budget: the main plan (admitted, lead, Thin and regular rays and steps, resolution, channels, iterations) is identical for every adaptive scale 0.30 .. 1.00 (15 values) and every display size (96 .. 2560 px) - 32 400 rows, 0 differences. The mutation that reserves from the per-frame target (what the previous round did) changes the plan in 126 of 432 (set, size, K, preset, budget) cells, so the test can fail. Literal: 1024 capacity at 16:9 reserves 1024 x 576, 2048 square 2048 x 2048, a malformed aspect clamps at 1:4. Static pins: the `liveAreaFeeds` extents come from `liveFeedReservationExtent(outputCapacity(slot), ...)` and from nothing named `mResolutionScale`, `gFPS` or `AppliedResolutionScale`; a header mutant halving the capacity fails test 36.
* **Thin coverage, default 1500 M, with fixed-capacity reservations** (admitted / K, how many of those march Thin, Thin rays; "rest" is the regular group; `probe4.out`). The earlier table in 11.4 used per-frame sizes and is replaced by this one:

| Screen, K | Feeds | Low | Medium | High | Cinematic |
|---|---|---|---|---|---|
| 1080p K4 | no live feed | 4/4, all Thin 4r | 4/4, all Thin 8r | 4/4, all Thin 9r | 4/4, all Thin 9r |
| 1080p K4 | 1 x 1024x576 (capacity 1024) | 4/4, all Thin 4r | 4/4, all Thin 8r | 4/4, all Thin 7r | 4/4, all Thin 8r |
| 1080p K4 | 2 x 1024x576 | 4/4, all Thin 4r | 4/4, all Thin 5r | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 1 x 2048x1152 (capacity 2048) | 4/4, all Thin 4r | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K4 | 8 x 2048x2048 (old worst case) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, all Thin 4r | 4/4, all Thin 8r |
| 1080p K8 | no live feed | 8/8, all Thin 4r | 8/8, all Thin 5r | 8/8, all Thin 5r | 8/8, all Thin 5r |
| 1080p K8 | 1 x 1024x576 (capacity 1024) | 8/8, all Thin 4r | 8/8, 1 Thin 8r + rest 8x12 | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 2 x 1024x576 | 8/8, 6 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 1 x 2048x1152 (capacity 2048) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 1080p K8 | 8 x 2048x2048 (old worst case) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, all Thin 4r | 8/8, all Thin 5r |
| 2160p K4 | no live feed | 4/4, 3 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 1 x 1024x576 (capacity 1024) | 4/4, 2 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 2 x 1024x576 | 4/4, 2 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 1 x 2048x1152 (capacity 2048) | 4/4, 1 Thin 4r + rest 4x8 | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K4 | 8 x 2048x2048 (old worst case) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) | 4/4, OFF plan (0 Thin) |
| 2160p K8 | no live feed | 8/8, 2 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 1 x 1024x576 (capacity 1024) | 8/8, 1 Thin 4r + rest 4x8 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 2 x 1024x576 | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 1 x 2048x1152 (capacity 2048) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |
| 2160p K8 | 8 x 2048x2048 (old worst case) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) | 8/8, OFF plan (0 Thin) |

Reading: the sizes are now a property of the camera (its capacity), so the quality of the main view no longer changes with where the camera is or how fast the viewer runs. A capacity-1024 camera costs the main view less than a capacity-2048 one (a 2048 projector output is the user's own choice); turning a feed on can still lower the main view's Thin quality, never its number of lights (section 11.4).

#### 12.2 Feed admission never falls when the allowance rises (`workloadWithin`, rung 6 is main-view only)

* **The defect (Codex).** Phase 0 applied the light-dropping rung (rung 6) of the P0.1 ladder to feed plans too. 720p Low K3, eight slots, a 512 x 512 feed: at 55 M the feed gets 4.1102 M and plans 3 lights at 2 x 8 (HEAD's plan); at 56 M it gets 4.2352 M and the P0 ladder, which now reaches rung 6 before HEAD's fixed-K fallback, planned 2 lights at 4 x 8 (HEAD: still 3, as HEAD's lights per draw change from 1 to 2). Section 11.7 had called this transition pre-existing; it was not (corrected there).
* **The fix.** `if (floors && drawn())` before rung 6: only the main view (`floors = true`) drops lights. A capture feed (`floors = false`) runs rungs 1-5 (a plan that fits all K lights at the floors is the same plan as before) and otherwise keeps HEAD's fixed-K fallback from the requested plan: rays and steps fall to 1 for all K lights, then fail closed. A feed therefore admits all K lights or none (never a count in between), and its admission is monotone in its allowance. The same holds for the Thin ON hybrid feed plan (`hybridWithin(..., floors = false)`).
* **Assertion 1 - the literal case** (test 36, part 1): 55 M and 56 M both admit 3 lights at 2 x 8; the allowance rises; `workloadWithin(..., 4.2352 M, floors = false)` admits 3 and the same call with `floors = true` still drops lights (2 at 4 x 8, never below the 4-ray floor).
* **Assertion 2 - monotone in the feed ALLOWANCE** (test 36, part 2): every preset x feed size (512 x 288 .. 2048 x 2048, 6 sizes) x K 1..8 x visible count {1, K} x allowance 0 .. 80 M in 20 000 steps, for the Thin OFF plan and the Thin ON hybrid plan: **1 536 384 rows, 0 admission drops when the allowance rose, 0 partial admissions** (admission is K or 0), and Thin ON >= Thin OFF at the same allowance in every row. The mutant that gives feeds rung 6 again fails test 36.
* **Assertion 3 - what is left over the BUDGET slider** (test 36, part 3): the main plan takes its share first (`regularCost(main)`), so the feed's allowance `(budget - regularCost(main)) / live feeds` itself can FALL as the slider rises. Over 4 mains x K {1,3,4,8} x 4 presets x 4 feed sizes x the whole 1..1500 M slider (384 000 rows, `feedWorkload` as the pipeline calls it) there are **2 955 admission drops for Thin OFF and the same 2 955 for Thin ON; every one coincides with a FALLING allowance and goes to nothing (none partial, none with a rising or equal allowance)**. These are the pre-existing reservation cliffs, an all-or-nothing effect of the main plan switching on:
  * HEAD, 720p Low K3, 512 x 512 feed: 11 M -> 12 M, 16 M -> 17 M and 22 M -> 23 M, each 3 -> 0 lights (HEAD's main OFF plan jumps up and takes the whole budget at those steps; allowances 0.684 M -> 0.118 M, 0.618 M -> 0.051 M, 0.676 M -> 0.110 M).
  * Current planner, same case: 14 M -> 15 M and 22 M -> 23 M (3 -> 0; allowances 1.750 M -> 0.032 M and 0.907 M -> 0.110 M). Mechanism at 15 M: one light at the floors needs 7.37 M for its single draw, but the per-draw cap is half the budget, so below 14.75 M the main plan draws nothing and reserves nothing; at 15 M it draws 2 lights (14.7 M), leaving (15 M - 14.7 M) / 8 = 31.8 K per feed. There is none at 55 M -> 56 M.
  * Over the wider grid of `probe1.cpp` / `probe2.cpp` (4 mains x K {1,2,3,4,5,6,8} x 4 presets x 4 feed sizes x 1500 budgets = 672 000 rows): HEAD 5 323 cliffs, the current planner 5 878; in both, all go to zero and all coincide with a falling allowance (0 with a rising one). The count differs because the P0.1 main ladder (resolution rung, floors) moves where the main plan switches on; the mechanism is HEAD's. Fixing the main-plan switch-on (a smoother reservation) is a separate planner change, deferred.
  * The sweep of test 25 keeps its assertions (Thin ON feed admission >= Thin OFF; no drop that OFF does not have) and its comment now says this.

#### 12.3 One live-feed snapshot per frame, passes admitted by feed identity (`FeedSet.id`, `FeedPasses::admit`)

* `renderAreaShadows` reads the live set from the Prism registry once per `gFrameCount` (`mAreaLiveFeeds`, `mAreaLiveFeedsFrame` in `LLPipeline`) and uses that snapshot for the main plan and for every feed pass of the frame. Opus confirmed the existing order is safe (the registry prepares every slot at the start of `renderAuxiliaryView`, and only one aux render runs per frame, before the main view); this makes it structural, so a feed that becomes live between the main plan and its own pass can no longer break "main + live feeds <= budget" (GLM, DeepSeek).
* `FeedPasses::admit(frame, identity, live)` replaces the count: a pass runs only for a feed that is in the frame's live set, and only once per frame, so one Gate source rendered repeatedly cannot use up another live feed's pass (GLM P2). The identity is the registry slot (`FeedSet.id`; `LLPrismLens::activeAreaFeedSlot()` for the pass being rendered, set by `ScopedActivePrismSlot`). A pass of a slot outside the snapshot (a preview-only update, a stale frame) draws nothing.
* Tests: math test 22 (a source rendered four times gets one pass, the next live feeds still get theirs, a feed outside the set gets none); the production dispatch fixture (`relight_vcam_dispatch_check.py`, real `renderAreaShadows` text): with all eight slots live a repeated slot-0 pass is refused 7 times and slots 1..7 are all admitted in the same frame, two live feeds admit exactly two passes, an unlisted slot is refused, and the registry is queried once per frame. Four new mutants are rejected (`unbounded_passes`, `passes_not_by_identity`, `live_feeds_ignored`, `live_set_read_every_pass`); header mutants fail test 22.

#### 12.4 Hybrid regular rays march the regular step count and are charged for it (`hybridPlan`, `areaShadowF.glsl`)

* The shader no longer clamps `area_rest_steps` to the Thin draw's `area_steps`; it clamps to 64, the largest uniform step count (the CPU already clamps the OFF plan to it). `hybridPlan` charges every Thin light `samples x max(Thin steps, regular steps)` in `perThin` (the fit test and the per-draw cap), the draw's channel count, `iterations` and `basisCost`; the far-step thinning loop stops once the Thin steps are no larger than the regular ones (further thinning saves nothing). The Python mirror of the planner (`thin_group`) follows.
* In the presets the regular count is below the Thin total (48 + 8 = 56 against 8 .. 32), so no preset plan changes; the case DeepSeek describes (regular steps above the Thin draw's) is Custom with more than 56 regular steps, and the case "regular steps above the Thin FAR steps" (Cinematic uniform 32 against a far floor of 8) occurs in preset plans.
* Tests. Math test 36, part 5: a synthetic OFF plan of 64 steps beside a Thin request of 56 total - two Thin lights are charged at 64, so a room sized for 56 (232 M) refuses two (265 M needed) and accepts one; the draw channel count and `basisCost` use the larger count; a Custom sweep (4 mains x K x 4 rays x 5 step counts x 2 resolutions x 9 budgets, 134 hybrids, **130 with the regular steps above the Thin far steps**) asserts that every plan is charged at least what its rays march and stays inside the frame budget. GPU (`relight_grain_check.py`, boundary): two more rows (Thin 4 x 56 beside regular 4 x 64, regular steps above the whole Thin draw; and Thin 4 x 56 beside regular 4 x 32, regular steps above the Thin far steps, the Cinematic uniform count), each with share 1 equal to the regular light byte for byte, filtered endpoint 0/255 (limit 1), continuity (a first attempt with 8 rays failed the fixture's own check that the rail separates the two marches, 96/255 against a minimum of 100, and was replaced by 4 rays); a shader mutant that clamps the regular steps to the Thin draw's breaks the identity of the first.

#### 12.5 Spectrum gate: blocks, rows and the drift on its own (`relight_grain_check.py`)

* **New reads of the shared residual** (after the smooth y-drift), beside the spectrum, the drift bound and the bands, for each plan (Low, Medium):
  * **Block structure.** The square root of the cross-covariance, between two independent halves of the seeds (even and odd), of the HIGH-PASSED block means (each block mean minus the mean of its four neighbours), for blocks of 4, 8 and 16 pixels. The plain block RMS cannot separate a block pattern from the honest noise of the seed mean (Low reads 1.9/255 at 4 x 4 with no mutation); the half-to-half covariance cancels the render noise, and the high-pass removes the smooth misfit of the plan's own penumbra (1.7/255 on the plain block means, equal at every block size). Honest reading: Low 0.31 / 0.63 / 0.95, Medium 0.14 / 0.32 / 0.24 /255; limits **0.8 / 1.2 / 1.5**.
  * **Row comb.** The largest single row mean: honest 0.41 (Low), 0.28 (Medium); limit **1.0**.
* **New mutants** (production-filter shader variants adding the structure to the final image, same chain), and which part fails them:

| Mutant | Spectrum peak (Low, Medium) | Block read 4 / 8 / 16, Low | Block read, Medium | Row read (Low, Medium) | Caught by |
|---|---|---|---|---|---|
| 4 x 4 blocks, random +-3/255 | 1.97, 0.81 (blind) | 1.71 / 1.27 / 1.15 (limit 0.8 at 4 x 4) | 1.71 / 1.17 / 0.75 | 0.76, 0.84 | block read (4 x 4) |
| 8 x 8 blocks, random +-3/255 | 1.95, 1.06 (blind) | 1.32 / 2.28 / 1.89 (limit 1.2 at 8 x 8) | 1.26 / 2.14 / 1.56 | 1.27, 1.38 | block read, and also the drift, the row read and the spectrum |
| +3/255 on every 16th row | 1.85, 0.66 (the honest values) | 0.37 / 0.64 / 0.95 | 0.25 / 0.37 / 0.24 | **2.83, 2.57** (limit 1.0) | row read |
| +6/255 on every 32nd row | 1.85, 0.66 (the honest values) | 0.42 / 0.65 / 0.97 | 0.32 / 0.39 / 0.31 | **5.48, 5.14** (limit 1.0) | row read |

  Opus's blind mutants are all caught: the 4 x 4 and 8 x 8 blocks by the block read, every 16th row (+3/255) and every 32nd row (+6/255) by the row read; the earlier stripes also trip the new reads, none passes.
* **The drift on its own.** The stripe loop never exercised the `SPECTRUM_DRIFT` assertion alone. A linear ramp in y across the patch (-3/255 .. +3/255), added to the rendered mean in memory (a shader ramp would clip against the 0 and 1 of the shadowed and lit sides of the patch and stop being a pure function of y), reads **no spectral line** (peaks 1.85 and 0.66, the honest values) and fails **only** the drift assertion (drift 3.10 and 2.96, limit 0.50): the test asserts both. The y-stripe 0.03 c/px mutant also trips it now.
* **Section 11.5 corrected** (above): neither the 8x8 block nor the 16th-row comb was caught by the previous gate.
* Honest spectrum unchanged: shared structure Low 1.85/255 (limit 2.0, margin 0.15), Medium 0.66/255; seed-mean detrend Low 1.77, Medium 1.11; drift 0.10 / 0.09 (limit 0.5); the half-bin diagonal mutant 2.48 / 2.49.

#### 12.6 Upsample fallback, own texel in the bilinear base alignment (`areaShadowFilterF.glsl`)

The pixel's own texel is now the nearest tap of the same four-tap neighbourhood the bilinear weights use (`base + (f >= 0.5)`, after the `areaTexelUV` base shift), instead of `floor(uv * res)`, which ignores the shift: the two agree on an even-sized target and differ on half of the pixels of an odd-sized one (255 wide: the trace texels sit on the even full-resolution pixels, an odd pixel is a tie). Tie rule: the higher texel, with the `areaTexelUV` tolerance (`f >= 0.5 - 1e-3`). GPU fixture: the thin wall and the thin object against the sky on an even (256 x 44) and an odd (255 x 45) target take exactly the texel an independent float64 model of the alignment names; the mutant with `floor(uv * res)` differs in 29 strip pixels on the odd target (wall and sky) and in none on the even one; the earlier mutants (no geometric gate; a filtered `textureLod`) still fail/are reported as before; pixels away from the strip are unchanged.

#### 12.7 Documentation, Coverage and small items

* "Live" (section 11.4, `pipeline.cpp`): a feed is live when the registry PREPARED it this frame at full quality. That includes a camera whose source is offline or invalid (it keeps a held publication and still reserves its share of the budget although it renders nothing), and it depends on the camera and the displays: a VCam display entering or leaving view, or the user switching a feed on or off, changes the live set and therefore the main plan (never the number of lights it shadows). The comment in `renderAreaShadows` said "a pass that is not in the set is refused"; it now describes identity admission exactly (12.3).
* `coverageText(soft, rendered, wanted, thin, thinRequested, lowMemory)`: the dead `planned` parameter is removed with the caller's argument and the `mAreaShadowPlanned` member (published, never read). `wanted` and `thinRequested` are still published before any early return; the wording and its tests are unchanged.

#### 12.8 Grain and cost

Grain (`final_grain.log`): unchanged, because no plan of the default grid changed: worst P0 reference error **6.76/255** (limit 8), noise 5.85, Medium 1080p K4 3.12 (limit 6), grazing floor 2.34 (pre-P0 7.59); the report-only line against the true 64 x 64 reference reads 51.02/255 worst (not gated, user decision 10/22). The planner mirror re-derives every row of the 2 x 4 x 8 x 4 x 2 grid, including the new step charge (`thin_group`).

Cost (RTX 5090, viewer not running, `final_cost.log`; the five plans are the same as in round 4, so differences are run-to-run noise): Medium 1080p K4 **1.114x** HEAD (0.923 vs 0.828 ms); Cinematic 4K K8 **1.315x** (5.400 vs 4.106 ms); Thin Medium 1080p K4 (pure Thin) **1.052x** (4.168 vs 3.963 ms); Thin Low 4K K4 hybrid (3 Thin 4 x 57 + 1 regular 4 x 8) **1.154x** (4.824 vs 4.180 ms); Thin Low 4K K8 hybrid (2 Thin 4 x 66 + 6 regular 4 x 8) **1.171x** (5.530 vs 4.724 ms); hard ceiling 2.5; the user-accepted target of 1.15x is exceeded at Cinematic 4K K8 and the two Low hybrids as before. The +50 percent rays control reads 1.58, 1.95, 1.56, 1.64 and 1.51 x its own ratio (limit 1.05). The feed change costs nothing at render time (it removes a rung from feed plans, never adds work); a feed now admits all K lights where it used to drop some below the floors, inside the same allowance and the same per-feed bound.

#### 12.9 Gates (each run once after the final edit; GPU gates one at a time; logs in `.tmp/ws_p0_r6/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full; includes the area suite with the production dispatch, contact, orbit, cost, grain, thin and rank) | **PASS** (900 s) | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** (15 s) | `final_shader_thin.log` |
| `relight_area_check.py` (production dispatch with 31 mutations incl. the 4 new ones, contact) | **PASS** (920 s) | `final_area.log` |
| `relight_thin_check.py`; `relight_off_check.py`; `relight_rank_check.py` | **PASS**; **PASS**; **PASS** | `final_thin.log`, `final_off.log`, `final_rank.log` |
| `relight_grain_check.py`; `relight_orbit_check.py`; orbit vs pre-P0 shaders; `relight_cost_check.py`; `relight_vcam_check.py` | **PASS** x5 | `final_grain.log`, `final_orbit.log`, `final_orbit_head.log`, `final_cost.log`, `final_vcam.log` |
| `relight_contact_check.py`, `relight_vcam_dispatch_check.py` | inside the area and shader checks above (no stand-alone entry point; `relight_area_check.py --dispatch-only` is a new developer shortcut that stops after the dispatch check, 2 min) | `final_area.log`, `final_shaders.log`, `dev_dispatch.log` |
| Standalone `alrelightmath` tests | **PASS 36/36** (new test 36; test 22 rewritten) | `math.log`, `math_compile.log`, `math_link.log` |
| Header mutations (`mutate_math.py`): feed rung 6 restored; Thin lights charged at Thin steps only; `basisCost` without the max; repeated feed pass admitted; unlisted feed admitted; reservation extent at half capacity | all six **rejected** (tests 22, 36) | `mutate_math.out` |
| `zs_check` per touched .cpp (pipeline, llprismlens, llfloaterdirector, alrelightmath_test) | **PASS** x4 | `zs_*.log`, `zs_rc.txt` |
| Three generators `--check` | **PASS** x3 (see the note) | `director_kit_gen_check.log`, `director_page_sources_check.log`, `director_functional_manifest_check.log` |
| `xui_layout_check.py --changed HEAD` | 0 findings (43 files) | `xui_layout.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **all gates OK** (it did not fail on the other session's `alframelimiter` files) | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

Notes: (1) the first full run failed on a fixture of this round (a synthetic 8 x 56 Thin beside 8 x 32 regular boundary row separated the two marches by 96/255 on the rail fixture, below that fixture's own minimum of 100); the row became 4 rays and every gate was rerun from the start. (2) `llfloaterdirector.cpp` is part of the Director kit generator's source bundle, so removing the `planned` argument changed the bundle hash in the header of the 41 generated files (XUI pages, `director_kit_index.xml`, `floater_director_v2.xml`, `gen_report.txt`); `director_kit_gen.py` was run once to regenerate them (only the hash line differs) and the three `--check` runs then pass. (3) Edits the permission system denied: none. Other sessions edit unrelated files of this tree (`alframelimiter.*`, `alderenderlist.*`, `llviewerdisplay.cpp`, menu XML, `indra/llwindow/*`, ...); none is touched by this change.

### 13. Fix round 6 (10/23) - change of plan: VCam feed soft shadows default OFF

**User decision 10/23:** "Let's test the main feed. We are waiting too long on vcam. We will cut the new vcam feature if we have to." **VCam feed soft shadows default OFF pending the feed-budget fix (user decision 10/23).** Round 6 was therefore stopped before any planner change landed; the tree's planner, tests and gates are exactly those of round 5 (section 12). Nothing of the HEAD-parity work below was kept.

* **The setting.** `RenderAreaShadowsVCam` ("Also in VCam feeds", Soft shadows card). Its default is `false` (it already was `false` at HEAD; only its settings.xml comment changed, to say that the default is OFF pending the feed-budget fix and what turning it on costs). The pipeline reads it in two places that both fail closed: `areaShadowsAllowedThisView()` (no feed pass is allowed) and `renderAreaShadows` (`if (feeds())` before the live-feed snapshot).
* **With it OFF no feed reservation or hold-back is applied.** `live_feeds` stays the empty `FeedSet`, `feedReservation()` returns 0 for an empty set, `feedSpend()` returns 0, so the main plan (OFF, Thin and hybrid) is planned against the WHOLE budget exactly as in a build without VCam feeds. `relight_vcam_check.py` runs the view policy in both states of the setting (a feed view is refused when it is off).
* **With it ON** the round-5 behaviour is unchanged. Known limitation (not fixed): at some budgets a live feed can get FEWER lights than at HEAD, and a feed switched on can lower the main view's quality.

#### 13.1 Findings of the round-6 reviews that are NOT fixed (for the later feed-budget fix)

* **Root cause (Codex P0, Opus P0, GLM P1).** The P0.1 OFF main ladder spends more of the budget than HEAD's OFF main (HEAD walked rays and steps down to 1 before it dropped anything), so the feed allowance `(budget - regularCost(main)) / n` shrinks: 720p Low K3, eight 512 x 512 feeds, 15 M: HEAD 3 lights, now 0; 1440p Cinematic K7, one live feed, 1500 M: HEAD 7, now 0. Section 12.2's "the mechanism is HEAD's" is wrong at 1500 M (a level loss, not a switch-on cliff), and the accepted `{15,23}` pin in test 36 hides the 15 M case.
* **A fix that was prototyped, swept and then REVERTED** (so it is a ready starting point): plan the OFF main plan inside `budget - feedReservation()` too (`workloadWithin` subtracts the hold-back for uniform plans as well; `workload()` passes `feedReservation` for every `q`; `hybridWithin` plans its `off` plan with the same hold-back, so `hybridWithin` and `feedWorkload` use ONE OFF plan; `hybridWithin` returns `off` for feeds, `floors = false`, before the hybrid loop - DeepSeek P1). With the round-5 `feedReservation` (sum over the live feeds of `min(allowance, need)`) a MIXED live set (for example 2048 + 1:1 + 4:1 feeds, or 512 + 2048) still drew fewer lights than HEAD (the allowance is one number for all feeds, so the reservation must cover the largest need); with `feedReservation = n * min(max_i need_i, (budget - c) / 8)` (c = the cheapest main plan, 8 = the slots HEAD divided by) the sweep (5 mains x 8 uniform marches x K 1..8 x 15 live sets x visible {K, 1} x every 13th budget, 1 113 600 rows, 3 712 000 feed rows) found 0 Thin ON < OFF, 0 non-monotone main, 0 frame-budget violations, 0 feeds below HEAD (eight slots) and 0 below HEAD at the live count. The full 1..1500 M sweep and the dependent math-test rewrites (tests 14, 15, 24, 29, 35, 36 assume the un-held OFF main) were not finished. Script: `.tmp/ws_p0_r7/sweep9.cpp` (HEAD planner copy: `head_ns.h`).
* **Liveness (DeepSeek P1 vs Opus).** By reading the code, not by a test: `prepareCameraCapture` runs for every slot every frame in `renderAuxiliaryView`, which `llviewerdisplay.cpp` calls BEFORE the main view, and stamps `mFrame.mFrame = gFrameCount` / `mPrepared` from stable state (the displays' visibility, `captureWorkRequired`), never from whether the scheduled render ran (that is a separate path, `render_one`). A slot that renders every 4th frame is therefore live on all frames; the claim that the plan oscillates at the capture cadence is not supported. A unit test for it (a pure predicate in `llprismlens.h`) was not written. Offline, unbound or invalid sources return before `mPrepared = true`, so they are NOT live and reserve nothing; the round-5 wording (section 12.7 and the `pipeline.cpp` comment, "includes a camera whose source is offline or invalid") is wrong on that point and still stands uncorrected in this build. A Gate crossfade can switch a source's inherited capacity 1024 <-> 2048 (`llprismlens.cpp` ~6334), so the live reservation, and with it the main plan, steps at the start and end of a crossfade. These matter only with the setting ON.
* **Not done:** the spectrum column-mean read (Opus: an every-16th-column +3/255 comb and a penumbra-confined row comb pass; the honest spectrum margin stays 0.15/255) and the Coverage-row fixes for cube/HUD views and stale numbers (DeepSeek P2).

#### 13.2 Gates of this build (logs in `.tmp/ws_p0_r7/`)

See the table in the reply to the lead; the gates run once after the last edit were: `relight_shader_check.py` (full), `relight_area_check.py`, `relight_off_check.py`, `relight_grain_check.py`, the math tests, `zs_check` on the touched .cpp files, the three kit generators `--check`, `xui_layout_check.py`, `prebuild_guard.ps1 -ChecksOnly` and `git diff --check`.

### 14. Gates of round 1 (historical; the current gates are section 12.9; logs in `.tmp/ws_p0_r2/`)

| Gate | Result | Log |
|---|---|---|
| `relight_shader_check.py` (full) | **PASS** | `final_shaders.log` |
| `relight_shader_check.py --area-only --thin-only` | **PASS** | `final_shader_thin.log` |
| `relight_area_check.py` (includes orbit, cost, grain, thin, rank) | **PASS** | `final_area.log` |
| `relight_thin_check.py`; `relight_off_check.py`; `relight_rank_check.py` | **PASS**; **PASS**; **PASS** | `final_thin.log`, `final_off.log`, `final_rank.log` |
| `relight_grain_check.py`; `relight_orbit_check.py`; orbit vs pre-P0 shaders; `relight_cost_check.py` | **PASS** x4 | `final_grain.log`, `final_orbit.log`, `final_orbit_head.log`, `final_cost.log` |
| Standalone `alrelightmath` tests | **PASS 34/34** | `math.log`, `math_compile.log`, `math_link.log` |
| `zs_check` per touched .cpp (pipeline, llfloaterdirector, alrelightmath_test) | **PASS** x3 | `zs_*.log` |
| Three generators `--check` (kit regenerated first, then page sources and manifest) | **PASS** x3 | `director_*_check.log` |
| `xui_layout_check.py --changed HEAD` | 0 findings (40 files) | `xui_layout.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS**, all gates | `prebuild.log` |
| `git diff --check` | **PASS** | `diff_check.log` |

## World shadows P1-lite (10/23): cube shadow maps for the top 1-2 lights (point or projector)

Branch `wip/cube-p1lite` (worktree `I:\alchemy-machinima-cube`), rebased onto the committed Phase 0 `e150b927cf4` (it was written on the mid-round snapshot `b0bb2d24f18`). Plan of record:
`RELIGHT_WORLD_SHADOWS_PLAN.md` rev 2, section 2 and "Phase P1-lite". Nothing pushed, built or stashed. All new settings default OFF and the OFF output is
byte-identical to the base (section 6). Runtime uniforms only: no define, no permutation, no reload, no listener.

### 1. What the user gets

Light > Rim > Soft shadows card, after "Thin objects": **World shadows** (switch), **World lights** (1-2, searchable as "world-shadow lights"), and a readout
("off", "choosing a light", "no light in view", "not enough video memory", "over the work budget at this size", "on 1 light (6 faces at 512)").
Help: "The most important light (point or projector) gets a 3D shadow map: blockers anywhere around it cast, even off-screen, and the shadow stays put while you cam. Six
shadow passes per light. Switch off and on to choose the light again." The SL status row appends "; World shadows still work" while the switch is on
(the world pass does not depend on `RenderShadowDetail`). Both settings join the Director scene allowlist and the take record (which also logs the
resident UUIDs and a per-channel `world` flag). Settings: `RenderAreaShadowCubeMaps` (bool, OFF), `RenderAreaShadowCubeLights` (S32 1-2, **default 2** since 10/23).

What changes in the picture, for the RESIDENT light only: its shadows come from blockers that are off-screen, behind the camera or hidden; its soft edge
stays fixed on the floor while the camera orbits; railings cast whole shadows; edges are sharp at contact and soften with distance (the source radius is
clamped at 0.25 m on the map side). Every other light keeps today's screen-space shadows. Attachment lights, actor-only lights, mirror copies,
VCam feeds and snapshots are out of scope (follow-ups); projectors were added on 10/23 (section 9).

### 2. Architecture (what was built)

| Piece | Where | Notes |
|---|---|---|
| Pure maths | `alworldshadowmath.h` | face table (probe cube order, `right = axis x up`, rows of `glm::lookAt` = `(right, up, -axis)`), widened FOV `2 atan(544/512)` = 93.47 deg, tile grid (3 x 2 tiles of 544 per light, 1632 x 1088 per light), receiver -> tile mapping, depth linearisation, bias constants, blind band `blindBandMax`, `contactDistance`, resolve ledger (pixels x lights x 48 gathers, ceiling 1.0 G), pinned residency, readout wording. |
| Residency (U8 Pinned) | `LLPipeline::renderAreaShadows` | Two session slots (never saved). An empty slot takes the best eligible light (point or projector, see section 9) of that frame's ranking not already resident; a slot clears only when its light stops being an eligible light in `mLights`, the planned count drops below its index, or the setting goes off. `ALAreaShadow::Candidate::world` orders resident lights first (`before()`); all flags false = the old order (720-permutation test). Main view only. |
| Cube raster | `LLPipeline::generateWorldShadowCubes`, called after `generateSunShadow` in `llviewerdisplay.cpp` | Six face passes per resident light in view, every frame, with `renderShadow` and its native defaults (so avatars, rigged meshes, alpha mask/blend, GLTF and grass cast as for a native projector), `RenderSpotLight = drawable` (`RenderSpotLight` only drops the emitting drawable's own sorted geometry: merged batches, double-sided glTF and hollow prims around the lamp still cast, so the near plane is what keeps a bulb's own shell out and `RenderAreaShadowWorldNear` is the escape hatch), a 544^2 D32F scratch target, then `glCopyImageSubData` into the atlas tile (keeps `DEFERRED_SHADOW_TARGET_WIDTH` and the full-target clear correct). Near = PER LIGHT, `max(RenderAreaShadowWorldNear (default 5 cm), 0.5 x the emitting object's smallest scale axis + 1 cm)`, clamped to 1 m and to 0.9 x the light radius (`ALWorldShadow::emitterNearPlane`; decoupled from `RenderAreaShadowLightBias`; a CUBE of that half-size around the light is left out, not a sphere). The raster (`generateWorldShadowCubes`) and the resolve (`world_info[].y`) call the same `worldShadowLightNear`, so the depth linearisation matches. The function restores the entry shader program and texture units 0..15 on every exit (`WorldShadowStateGuard`), and under gDebugGL compares the whole snapshot including the program and texture bindings, far = 1.5 x the light radius. Camera IDs `CAMERA_WORLD_SHADOW0/1` are reused by the six sequential passes of one light: verified safe, the shadow culler's `earlyFail` never reads the per-camera visibility stamps and every pass rebuilds its `LLCullResult`; the Prism aux pass reuses `CAMERA_SPOT_SHADOW0+i` the same way. |
| State hygiene | same | Saves/restores modelview, projection, GL matrix stacks, viewport, render-target resolution, colour mask, scissor, camera ID, `RenderSpotLight`. Under `gDebugGL` a before/after snapshot logs `[RelightQ] state-restore FAIL <item>`. Tracy zones `WorldShadow::render/light/face` + a GPU zone; CPU ms in `mWorldShadowCpuMs`. |
| Atlas | `mWorldShadowAtlas`, `mWorldShadowScratch` | 6.77 / 13.55 MiB + 1.13 MiB scratch, allocated lazily for the planned light count, compare mode NONE, nearest, clamp (set in `generateWorldShadowCubes`, NOT in `allocateShadowBuffer`). Released lazily on the OFF transition (the only GL call on the OFF path); allocation failure = no cube this frame, status "not enough video memory", retried next frame, the channel falls back to the full screen trace. |
| Validity | `worldShadowOwned()` | A channel is world-owned only when its slot's stamp equals `gFrameCount` and the UUID matches (a validity stamp only, like `mSpotShadowFrame`). The cube renders the frame AFTER the latch, so there is one frame of screen trace at the latch. |
| Gather | `areaShadowF.glsl` | `uniform vec4 area_world`: the mix weight becomes `max(contact_fade * native_strength, area_world)`, so a world channel outputs the CONTACT term. For a world channel the march thickness tightens to `max(4 footprints, 5 cm)` (the user's 0.35 m accepted silhouettes in front as contact blockers: camera-dependent false-dark spots). Host: contact distance `clamp(max(mapped contact, blind band), 0.02, 0.5)`, dropout 0. |
| Resolve | `areaShadowFilterF.glsl`, upsample branch only | Full resolution, never blurred: `out = min(upsampled contact, worldVis)`. Face = major axis; normal offset `min(1 texel, 5 cm)` along the geometric normal; compare against the **receiver-plane depth of each tap** (exact plane through the offset receiver, capped at 8 per texel of tap distance) plus `b = 3 mm + texel_w * min(tan, 8)`; 16-tap blocker search (Vogel disk with the centre, radius `r_map (z_r - z_s)/(2 z_s z_r) * 512` texels, 1-12), penumbra radius `r_map (z_r - z_b)/z_b * 512/(2 z_r)` (1-12 texels, 12 + 2 <= the 16 texel gutter), 32 bilinear compares by `textureGather` addressed at exact texel corners. Rotation: R2 rank of the receiver's face texel + the UUID hash (camera-invariant, no time input). Guarded by `#if __VERSION__ >= 400`. A pixel at window depth 0 (clamped to the near plane) is skipped. |
| Uniforms | `pipeline.cpp` | `area_world` is uploaded on every pass that has a world channel and once with zeros on the pass after the last one (a feed pass, a toggle), so no stale channel survives and a steady OFF frame makes no extra GL call. `AREA_WORLD_ATLAS` is a reserved sampler (appended after `RELIGHT_ENABLED`). `area_world_rot` is three row vectors (no new matrix API). |

### 3. Deliberate differences from the brief (with the harness evidence)

1. **Per-tap receiver-plane depth** instead of one constant slope bias. With the brief's constant bias the floor of scene A shadowed itself across the filter
   footprint (floor texels toward the light counted as blockers, shrinking the penumbra): scene A mean MAE 0.184, flicker 0.096. With the plane depth: 0.053 then 0.039 (section 5).
2. **Normal offset 1.0 texel**, not 1.5 (sweep on A+B: 1.5 -> A 0.053 / B 0.044; 1.0 -> 0.039 / 0.030; 0.5 -> 0.028 / 0.017 but camera flicker rises on B; 0 -> 0.039 / 0.013 with A flicker 0.096). One constant set for every scene.
3. **Tightened contact thickness** for world channels (above). Camera-only flicker of scene B fell from 0.212 to 0.057.
4. **`B_max` uses a tan cap of 2** (receivers up to ~63 deg from the light), 0.12 m at 10 m, so the contact distance stays 0.15 m (the user's mapped-contact default) instead of saturating at the 0.5 m clamp.
5. **Rotation (b) kept.** (c) fixed disk: A worst-pose edge p95 60 px (banding hole) against 12.4 px, MAE equal (0.038 vs 0.039), B flicker equal (0.056 vs 0.057). Decision recorded in `.tmp/cube_r1/full1.log`.
6. Nothing was cut: N = 1-2, the blocker search and the variable-radius PCF are all in.

### 4. 3D harness (`relight_3d_reference.py --shader-ref WORKTREE --source scripts/code_tools/relight_3d_cube_source.py:CubeSource`)

`CubeSource` stands in for the RASTER only: a CPU ray cast of six widened 544^2 faces per resident light (the shared face table, front faces only, near cube clipped,
the emitting prim excluded, cached per light set) uploaded as a D32F atlas. The WORKTREE gather + filter shaders are the production ones, driven with exactly the uniforms
`renderAreaShadows` uploads. Configs (10/23): `cube` (the setting default, N = 2, near 5 cm, emitter excluded), `cube+thin`, `cube1` (N = 1), diagnostics `cube_worldonly` (no contact term), `cube_fixedrot`,
`cube_near025` (the superseded 25 cm rule), `cube_near010`. The 10/23 round 4 harness has NO explicit emitter exclusion (the production hook cannot reach batched or double-sided geometry): the near plane is per light (`emitter_near`), and the only skip is the back-face cull of a single-sided closed prim that encloses the light (`Prim.cull = 'back'`). `CUBE_POINTONLY=1` restores point-only residency. Residency in the harness is
the highest production importance of the pose (unit luminance); scenes A and B have one light, so it cannot change inside their camera groups.

### 5. Results (FIRST round, N = 1 and the 25 cm near rule; the current numbers are in section 11; logs `.tmp/cube_r1/full1.log`, `allk.log`; screen-space baseline = P0 `Default`: B 0.085, A 0.20)

| Gate | `cube` | `cube+thin` | Limit |
|---|---|---|---|
| B mae_on mean / worst pose | 0.030 / 0.069 | 0.030 / 0.069 | 0.051 / 0.145 |
| A mae_on mean / worst pose | 0.039 / 0.071 | 0.039 / 0.071 | 0.12 / 0.238 |
| A camera-only flicker p95 worst / pop | 0.043 / 0.005 | 0.045 / 0.005 | 0.05 / 0.05 |
| B camera-only flicker p95 worst / pop | **0.057** / 0.013 | **0.077** / 0.013 | 0.05 / 0.05 |
| false_dark (acne) worst, A / B / C | 0.005 / 0.000 / 0.003 | 0.012 / 0.000 / 0.015 | 0.02 |
| leak_on worst, A / B | 0.008 / 0.000 | 0.004 / 0.000 | 0.05 |
| C lamp pool dark | 0.000 | 0.000 | 0.02 |

* **Flicker misses its gate narrowly on scene B `camorbit_low`** and is measurement-limited, not a world-space dependence: with the contact term off it is 0.048; with a 2x wider
  penumbra it falls to 0.031. The grazing floor pixel is ~3 cm across against an ~8 cm penumbra, so the probe's bilinear read of pixel centres alone moves the p95 by ~0.05. The Thin plan's
  extra 0.02 is the Thin contact march (screen) in that group.
* **Ratchet (tightest HEAD limit) on R and C**: `cube` passes A, B and C (C false_dark 0.003 vs 0.0091; `cube2` C: mae 0.062, leak 0.012). It FAILS on R (mae 0.437 > 0.218, leak 0.322,
  false_dark 0.035, pen_cover_far 0.606) and `cube+thin` fails C false_dark 0.0149 > 0.0091, because the second light of R is a PROJECTOR (and of C the non-resident lamp keeps the Thin
  screen march): with `cube_allkinds` R passes everything (mae 0.031, worst 0.055, leak 0.011, acne 0.002). Projector world tiles are P4.
* **leak_exp** (off-screen / hidden blockers that are fully dark in the ground truth but left lit; the brief put the screen-space baseline of R/C near 80 %): R 0 % when every light has a cube (`cube_allkinds`; `cube` shows 24 % because the projector's blockers stay unseen), C 0-1 %, A 3 % (0 % with Thin), B 0 %.
* Determinism: `relight_3d_cube_source.py --determinism` = 21 (pose, config) pairs byte-identical on repeat.
* Contact `false_dark` at one pose of C (camera inside a wall, pixel depth clamped to the near plane) was a harness artefact; the resolve skips depth 0.

### 6. Other gates

| Gate | Result |
|---|---|
| OFF identity, `relight_cube_off_check.py` | **724 / 724** (181 poses x 4 configs) RGBA8 outputs byte-identical between `b0bb2d24f18` and WORKTREE with `area_world = 0`; mutation (non-zero `area_world` + blocker atlas) differs; static: no GL call before the early return except the guarded lazy release, uniforms guarded, no reload listener or shader-manager dependency |
| Static pins, `relight_world_check.py` (also run by `relight_shader_check.py`) | face table C++ == GLSL == harness mirror; constants (tile, gutter >= kernel + 2, bias set, 16 + 32 taps = the ledger); the render-type mask equals `generateSunShadow`'s verbatim; section 10: no diff in `allocateShadowBuffer`, `generateSunShadow`, `generatePrismSpotShadows`, no auction/slot-count line |
| Cost, `relight_cost_check.py --world` | 1080p K4 Medium (8 rays x 12 steps, half), area pass N=1 1.11x, **N=2 1.18x the P0 pass** (ceiling 2.5x); resolve N=2 0.16 ms at 1080p, **0.59 ms at 4K** (ceiling 3 ms), worst case (every pixel runs all 48 gathers) 1.29 ms at 4K. Raster: 12 face passes at N=2, structural; in-viewer Tracy is report-only and still to be taken (D1) |
| Tests | `alrelightmath_test.cpp` test<37>-<40> (face table, FOV, tile slots, depth, ledger, bias, readout, residency, `before()` permutation identity); run through a standalone shim (`.tmp/cube_r1/tut/mk.py`): PASS |
| Compile | `zs_check` (copy re-pointed at the worktree: `.tmp/cube_r1/zs.sh`) PASS for `pipeline.cpp`, `llviewerdisplay.cpp`, `llfloaterdirector.cpp`, `llshadermgr.cpp`, `alrelightmath_test.cpp` |
| Generators | `director_kit_gen`, `director_page_sources`, `director_functional_manifest` with `--check` PASS; row pin +3; `xui_layout_check` 0 findings; `ui_draw_lint` no new findings |

### 7. Not done / open risks (for D1)

* The viewer was not built or run. In-world checks needed: the sphere + flying-light orbit, the railing-to-floor orbit, avatars and attachments casting (production casters), a Tracy capture in a busy avatar scene (CPU/GPU ms of 6 face passes), `glCopyImageSubData` availability (a missing function reads as the allocation-failure status), and a 1000-frame `gDebugGL` state-restore run.
* One frame of screen trace at the latch (the cube renders the frame after the latch); a resident light that leaves the view stops rendering and re-renders the frame it re-enters.
* Contact is still the existing half-resolution march (P3 replaces it); the world term is written to 8-bit visibility planes.
* Follow-ups, as in the brief: Re-pick button, attachment/face lights, feeds, snapshots, projectors (P4), per-pixel `d_c` (P3), `glPolygonOffset`, VO-cache counts (U15), seam and production-caster fixtures in-world, GPU timestamps for the raster.
* Review: Codex + Opus + GLM + DeepSeek reviews of this change are still to be run (CLAUDE.md), then ONE build.

### 8. Scene A2 (railing + receivers behind it) and the existing suite

**A2** (`relight_3d_scene.py:scene_railing_objects`, harness only): scene A plus a 0.8 m crate (near face 1.0 m behind the railing) and a 0.5 m sphere on the
far side from the light, so the railing's shadow falls across a vertical face, a curved surface and wraps from the floor onto them; both also cast onto the
floor. Same 72 orbit poses as A. 72 poses, `cube` / screen-space `Default` (P0 base shaders): mae_on mean **0.039** / 0.267, worst pose 0.073 / 0.379, leak worst
0.030 / 0.731, acne worst 0.006 / 0.054, camera flicker p95 0.053 / 0.745, penumbra coverage near/far 0.91 / 0.92. Thin objects screen march (`Default+Thin`):
0.170 / worst 0.231 / leak 0.356. Contact sheets (ground truth | screen-space | cube maps, plus error views) for 6 poses of A2 and of A: `.tmp/cube_r1/sheets_A2/`
(`scripts/code_tools/relight_3d_cube_sheets.py`). Logs: `a2cube.log`, `a2screen.log`.

**Existing relight suite** (run with `HEAD` pointed at the pre-P0 commit `4dd9ff661c1` by a temporary `git reset --soft`, because the checks diff against `HEAD`, and in this
worktree HEAD is the base snapshot): `relight_off_check`, `relight_rank_check` (its pin of the gather's `areaTrace` call now follows the per-light
`thickness`, which is `area_params.y` for every channel except a world-owned one), `relight_thin_check`, `relight_orbit_check`, `relight_contact_check`,
`relight_cost_check` PASS. `relight_grain_check` FAILS the same way on the unmodified base snapshot (`8x56 Thin beside 8x32 regular half: the rail fixture must separate
the regular and the Thin march (max difference 96/255)`, threshold 100, reproduced in a clean worktree of `b0bb2d24f18`); it is a P0-round-5 matter and not part of this
change. `relight_area_check` chains after it and is therefore not reached.

### 9. Round 2 (10/23): projectors, the rebase, B flicker

* **Projectors are world-eligible** (user decision 10/23: the top-ranked lights get world shadows whether they are point lights or projectors). Rule chosen for section 10:
  a projector that owns a SAMPLED native SL shadow map at that moment (`spotShadowMapSampled`) keeps its existing "SL map + contact" path: it is never chosen, never
  world-owned and its six cube passes are skipped while it does; it ranks like any other light. A pinned projector that later acquires a slot simply falls back to its native
  path for as long as it holds it (no double edges, the native auction and fades untouched), and is world-owned again when the slot goes. Readout wording: "no light in view".
  Harness (no native maps there): scene R, N = 2 (both lights get a cube): mae 0.031 / worst 0.055 (was 0.437 with the projector on the screen path), leak 0.011, acne 0.002;
  the Default setting N = 1 shadows one of the two lights only, so R passes the ratchet at N = 2 and not at N = 1 (one of its two lights stays screen-space).
* **Rebased** onto `e150b927cf4` (conflicts only in generated kit files, regenerated). The existing suite passes with `HEAD` pointed at the pre-P0 commit `4dd9ff661c1` (the checks
  diff against it): `relight_shader_check` FULL chain (area, thin, rank, grain, orbit, contact, cost, `relight_world_check`) exit 0, `relight_off_check`, `relight_cost_check`.
  The grain check's rail-fixture failure was a mid-round artefact of the old base, as expected.
* **B camera flicker** (0.057 on `camorbit_low`, gate 0.05) is documented as MEASUREMENT-LIMITED, not fixed: contact term off 0.048; penumbra twice as wide 0.031; the contact term
  adds 0.009 and the Thin plan's extra contact march 0.020. No principled change was found (anything that lowers it widens or blurs the penumbra and costs accuracy, or clips contact noise).

### 10. Scene W: a lamp in a shade in a corner, and the face near plane

Scenes `W` (open-bottom shade) and `WS` (the light SEALED in a 0.2 m box housing with an open bottom face) model the user's corner (`.tmp/cube_r1/ref_user/`): a wall lamp 0.12 m off the wall
2.2 m up, above a recessed door, a roof overhang 0.6 m out 1.2 m above the lamp, a louvered vent on the adjacent wall (two blocks of 10 slats, 1.5 cm thick, 4 cm apart, 4 cm proud; the
upper block level with the lamp, the lower block 1 m lower), crates, a 0.3 m pipe, a railing. The emitter is a tiny separate `bulb` prim; the shade/housing is a different prim, so it
BLOCKS in the ground truth: the wall above the lamp is DARK. 43 poses each (eye and low orbits looking up at the wall above the lamp, a dolly, an arc around the vent). Report:
`relight_3d_corner_report.py`, sheets (ground truth | screen-space | cube | cube near 5 cm) in `.tmp/cube_r1/sheets_W/`, logs `w_cube.log`, `w_screen.log`, `w_report.log`.

| Region / scene | ground truth | screen-space today | screen, Light bias 5 cm | cube, near 25 cm (shipped rule) | cube, near 5 cm | cube, near 10 cm |
|---|---|---|---|---|---|---|
| W whole-scene mae_on | | 0.758 | | 0.750 | **0.103** | 0.475 |
| WS whole-scene mae_on | | 0.863 | | 0.858 | **0.055** | 0.487 |
| W/WS wall above the lamp: mean visibility (GT-dark pixels left lit) | 0.000 | 1.000 (100 %) | 0.988 (98 %) | 1.000 (100 %) | **0.000 (0 %)** | 0.909 (90 %) |
| W upper vent (level with the lamp), mean visibility (GT-dark left lit) | 0.012 | 0.984 (99 %) | 0.948 (93 %) | 0.940 (92 %) | **0.013 (1 %)** | 0.695 (52 %) |
| W lower vent, louver mae / contrast std (GT 0.381) | mean 0.270 | 0.295 / 0.442 | 0.320 / 0.438 | 0.069 / 0.450 | 0.070 / 0.452 | 0.070 / 0.452 |
| WS lower vent mae (GT mean 0.101) | | 0.447 | 0.471 | 0.232 | **0.096** | 0.187 |

Findings: (1) with the shipped rule (near = max(5 cm, Light bias 25 cm)) the cube loses the effect completely: the shade and housing panels sit 9-11 cm from the light, inside the clipped cube, so the wall above
the lamp reads 1.0 where the truth is 0.0. (2) Today's screen-space ALREADY loses it (1.000): the gather skips occluders within the Light bias of the light centre, and lowering that to 5 cm barely helps (0.988), so
the screen trace cannot see through to the shade at all; the user's 10/21 build, which darkened the wall, predates the global housing bias. (3) The cube with the near plane at 5 cm restores it exactly (0.000, mae 0.103 / 0.055), keeps the louver
shadows on the lower vent (contrast 0.45 vs 0.38) and is the only variant that darkens the upper vent correctly. (4) 10 cm is fragile: the panels sit at 9.5-10.5 cm, so it is a partial loss (0.909 lit).
Side effect of near 5 cm: a thin false-dark (blue) band at the lower edge of the lit pool on the side wall in some poses (worst pose mae 0.147 / 0.114).

Options (NOT implemented; the lead decides): **A** near fixed at 5 cm, decoupled from `RenderAreaShadowLightBias` (which keeps governing the screen trace only): the numbers above, one line in `nearPlane()` and its call sites, no new setting.
**B** near = max(5 cm, a per-light distance taken from the emitting prim's own bounds): collapses to A here (the emitter is a 2 cm bulb) and is moot for a large emitting prim, which `RenderSpotLight` already excludes by identity; it adds code for no measured gain.
**C** keep 25 cm and add a "World near" slider (default 5 cm, advanced) as the escape hatch for fixtures that must not block their own light (a sloppy globe mesh wrapping the bulb as a SEPARATE prim would now correctly cast): A plus the old behaviour on request. Recommendation: A, with C as the follow-up if a scene needs it.

### 11. Round 3 (10/23): near plane 5 cm, emitter never casts, N = 2 default, final numbers

**Decisions applied.** (1) The face near plane is a FIXED 5 cm (`ALWorldShadow::kNearPlane`), decoupled from Light bias (which keeps governing the screen trace only). (2) The light's own
emitting prim is NOT reliably excluded (see the limit below; the claim made in the first draft of this section was false). (3) The default number of world lights is **2** (`RenderAreaShadowCubeLights`, settings default, kit default, harness `cube`).

**How the emitter exclusion works (it is NOT a guarantee), and the limit of it.** `RenderSpotLight` filters only the per-drawable sorted geometry, never the merged batches, and double-sided glTF or hollow prims face the light, so a bulb shell can cast into its own cube. Escape hatch: `RenderAreaShadowWorldNear` (F32 metres, default 0.05, clamped 0.02 to 1.0, Debug Settings; no Director row): geometry closer than this to the light never casts. Raise it if a lamp inside a hollow or double-sided bulb goes dark. The existing hook is `LLPipeline::RenderSpotLight` (set to the light's drawable around every cube pass; `stateSort(LLDrawable*)`
skips it). That hook only reaches geometry that is enqueued PER FACE (alpha pools, avatars, trees); the opaque, mask and GLTF geometry the shadow pass draws comes from the spatial groups' draw
infos, which are merged batches of many prims (`LLDrawInfo` has one vertex range, no per-prim list), so there is NO per-object exclusion hook for batched draws, and building one means splitting index
ranges inside the ~13 draw sites of `lldrawpool.cpp` (not minimal, not testable without the viewer). What does hold, with no code: the shadow pass renders FRONT faces only, so a CLOSED prim that
ENCLOSES the light (a glass bulb, a sealed box) is back-facing from inside and casts nothing. This was the FIRST-DRAFT argument and it is NOT sufficient: a double-sided glTF bulb (culling is disabled for double-sided materials) or an inward-wound bulb faces the light and fills every face. Round 4 (Codex P0 2) replaces it with the per-light near plane above: scenes WB (single-sided), WD (double-sided) and WI (inward-wound) put the light at the centre of a 16 cm bulb that is the emitter; none may black out the pool below the lamp and the shade (a separate prim 10.5 cm out) must still darken the wall above. The case that is NOT covered: an emitting prim that does not enclose the light (the emitter IS an open shade or cone mesh): it still casts in the batched passes. Follow-up options if that
fixture shows up in-world: an emitter-OBB test in the resolve (skip blockers whose reconstructed world point lies inside the emitter's oriented bounds, about 20 flops per gathered texel, fragile where
the shade touches the bulb), or per-prim index-range splitting in the batch draws.

**Scene WB** (`W` with the light inside a closed 0.10 m glass-bulb sphere that IS the emitting prim, plus the separate open-bottom shade). Region metrics, `cube` (near 5 cm) against today's screen-space
and the superseded 25 cm rule (`w_report.log`, sheets in `.tmp/cube_r1/sheets_W/WB_*.png`):

| WB region | ground truth | screen-space today | cube near 25 cm | cube near 5 cm (shipped) | cube, no explicit exclusion |
|---|---|---|---|---|---|
| wall above the lamp, mean visibility | 0.000 | 1.000 | 1.000 | **0.000** | 0.000 |
| pool below the lamp (door + wall, GT LIT), mean visibility / MAE | 0.693 | 0.989 / 0.296 | 0.999 / 0.306 | **0.740 / 0.094** | 0.740 / 0.094 |
| upper vent (level with the lamp), mean visibility | 0.012 | 0.983 | 0.940 | **0.012** | 0.012 |
| lower vent louvers, MAE (contrast std GT 0.381) | 0.270 mean | 0.296 (0.442) | 0.069 | **0.069 (0.450)** | 0.069 |

The bulb does not black the light out: WB's pool and louver numbers equal W's. W, WS and WB whole-scene mae_on: 0.103 / 0.055 / 0.103 (worst pose 0.147 / 0.114 / 0.147); the 25 cm rule: 0.750 / 0.858 / 0.750.
The harness' own acceptance for the corner is in `relight_3d_cube_gates.py` (wall above the lamp <= 0.05 mean visibility, lower-vent louver MAE <= 0.10).

**Round 5 (10/23): ROOT CAUSE of the in-world wedges, the Chebyshev near plane, status honesty and diagnostics.**

* **Root cause (Codex, real-GL reproduction).** The resolve's view -> agent rotation was uploaded with a `memcpy` of a `glm::mat3`. With the aligned glm types the mat3 columns are 16 bytes wide, so a 36 byte copy scrambled the three rows:
  51 % of the floor pixels read dark instead of 9 %, with NO emitter geometry at all (40.9 % false floor shadow with the casters removed). Fixed by `ALWorldShadow::packViewRotation` (explicit `out[3*column + row] = view[column][row]`).
  The 3D harness packs the same rotation in NumPy (`pack_view_rotation`, the same rule), which is why it never saw the bug; the unit test pins a nine-number vector that `relight_world_check.py` compares with the harness packing, and carries a NEGATIVE control
  (the old aligned memcpy must not rebuild the agent axes).
* **Near plane = the emitter's world-AABB Chebyshev reach.** The six faces clip by AXIAL depth, so the cleared region is the world-axis CUBE |p|inf < near, not a sphere. Round 4's inscribed radius (half the smallest scale axis) cleared spheres and axis-aligned cubes only: an elongated
  double-sided bulb (0.10 x 0.10 x 0.16) blacked out a ~30 degree cone straight down, a 0.4 x 0.4 x 0.1 panel receivers 50-76 degrees off vertical, and a rotated box failed the same way. Now `near = max(RenderAreaShadowWorldNear, reach + 1 cm)`, `reach = max_i max(hi_i - p_i, p_i - lo_i)` over
  `drawable->getSpatialExtents()` (the prim's WORLD AABB, so rotation is already in it), clamped to 1 m and 0.9 x the light radius. A light on a link of a linkset also takes the reach of small SIBLING links (AABB contains the light, largest extent < 1 m): a tiny invisible light link inside a
  double-sided bulb link. Raster and resolve call the same `worldShadowLightNear(light's drawable, getLightRadius())` (the light's own drawable matched by UUID, never the root's), so they agree for child prims (GLM's linkset-scale P0 was refuted: the unit test and a static pin cover it).
* **Point lights are omnidirectional, projectors take their direction from the prim rotation.** The cube is visibility only: its faces are world-axis aligned for any projector rotation (the cone/texture belong to the normal light path). `relight_world_check.py` pins that the cube path reads no rotation and
  that a projector pointing 30 degrees down at 45 degrees yaw builds the same near and the same six faces as the point light at that position.
* **Back-face culling is NOT a safe assumption.** In-world, a rotated 0.5 m SINGLE-SIDED box that emitted a light seemed to cast from inside. The scrambled rotation alone explains the wedges, so the cause of that observation is not proven; the shadow pass culls GL_BACK, but double-sided PBR, mesh and
  inward-wound surfaces face the light. The harness therefore treats a `cull 'back'` prim that encloses the light as casting nothing and every `cull 'none'` prim as casting from inside, and the near plane covers both (the 0.5 m box turned 45 degrees: reach 0.354 m, near 0.364 m).
* **Gates (`relight_3d_cube_gates.py --report`).** Every corner scene must be present (W, WS, WB, WD, WI and the new WE elongated double-sided bulb, WR rotated box, WP 0.4 x 0.4 x 0.1 panel with a wider hood, WK light link inside a double-sided bulb sibling); a missing scene or region FAILS.
  The pool below the lamp must stay within 0.05 of the ground truth (a dead lamp passes the wall-above check with 0; it cannot pass this one), and the shade must still darken the wall above (<= 0.05). Mutation: `RELIGHT_CUBE_NEAR_RULE=inscribed` restores the round-4 rule and the gate FAILS (see the results below).
* **Status honesty.** The World shadows readout counts the lights that RENDERED a cube this frame, names the pinned lights (the object name when the object is selected, else `light <uuid8>`) and says why a pinned one is not rendering: light not found, not eligible, native map, out of view, range inside the near plane.
* **Diagnostics.** (1) `RenderAreaShadowWorldDebug` (Director: World view, advanced; runtime only): 1 = the cube term alone, 2 = face index as grey (face / 5), 3 = receiver depth minus atlas depth (0.5 = the recorded surface, white = a blocker in front, black = nothing recorded). The lights that own a cube write it into their
  visibility; read it with Visibility view. 0 (the default) is byte-identical (uniform default 0, OFF identity unaffected). (2) A throttled log (at most one line per 2 s while World shadows is on): pinned UUIDs and names, job count, per-job near/far/origin, the first glGetError after each glCopyImageSubData, the state.
  (3) The scratch clear runs under `LLGLDepthTest(GL_TRUE, GL_TRUE)` so the depth write mask can never leak a previous face into a tile.
* **RenderAreaShadowWorldNear is a last-resort override** (a floor, Persist=1). The emitter is cleared automatically; a value close to the light's height above the floor puts the floor ON the near plane (no receiver beyond it: the floor under the lamp reads lit, the shadow looks cast from below). Leave it at 0.05.

**Pool-edge band at 5 cm (the thin false-dark band at the lower edge of the lit pool).** Cause identified, not cheap to remove: a blocker 10 cm from a 5 cm source throws a penumbra about 1.2 m
wide at 2.4 m, the resolve caps the PCF radius at 12 texels (about 0.11 m there, bounded by the 16 texel gutter), so the pool edge is sharper than the ground truth's. That is the P2 wide-penumbra
hand-off (plan section 2.6.3, U16), not a bias or near-plane effect; raising the cap needs a larger gutter or the screen-space hand-off. Left as is and documented.

**A fix that belongs to the harness, not the renderer:** the gather also gives a world channel no contact term at a pixel whose window depth is 0 (clamped to the near plane, as the viewer clips such
geometry): scene C pose 12 puts the camera inside a wall, and the Thin contact march false-darkened those pixels (C `cube+thin` false_dark 0.0146 > 0.0091). OFF is unchanged (the guard needs `area_world`).

**B camera flicker acceptance (explicit, user decision 10/23).** Target 0.05; measured 0.057 (B `camorbit_low`, A2 0.053; A 0.043). It is MEASUREMENT-LIMITED: contact term off 0.048; a penumbra twice as
wide 0.031; the cause is the grazing floor pixel (~3 cm) against an ~8 cm penumbra, not camera dependence of the world term. `relight_3d_cube_gates.py` holds A to 0.05 and B and A2 to 0.06, a recorded
relaxation (the note sits in that file's docstring), and keeps the pop limit at 0.05 everywhere.

**Final harness table** (`cube` = N 2, near 5 cm; `.tmp/cube_r1/full3.log`; mae_on mean / worst pose, camera flicker p95, acne worst, leak worst):

| Scene | mae_on mean / worst | flicker p95 | acne | leak | screen-space today (Default) |
|---|---|---|---|---|---|
| A | 0.039 / 0.071 | 0.043 | 0.005 | 0.008 | 0.20 (brief) |
| A2 | 0.039 / 0.073 | 0.053 | 0.006 | 0.030 | 0.267 / worst 0.379 |
| B | 0.030 / 0.069 | 0.057 | 0.000 | 0.000 | 0.085 (brief) |
| C | 0.062 / 0.069 | 0.070 | 0.002 | 0.012 | not re-measured |
| R | 0.031 / 0.055 | 0.082 | 0.002 | 0.011 | not re-measured |
| W / WS / WB | 0.103 / 0.055 / 0.103 (worst 0.147 / 0.114 / 0.147) | n/a | 0.003 / 0.000 / 0.003 | 0.013 / 0.016 / 0.013 | 0.758 / 0.863 / 0.758 |

R and C pass the HEAD ratchet for `cube` and `cube+thin` at the default N = 2 (`cube1`, N = 1, still fails R: one of its two lights stays screen-space; reported for comparison).
(Screen-space column: A and B are the brief's P0 baselines, A2 and W/WS/WB were measured in this work; C and R were not re-measured.)

**Cost at the default N = 2** (`relight_cost_check.py --world`, 1080p K4 Medium): area pass 1.17x the P0 pass (ceiling 2.5x); resolve 0.16 ms at 1080p and 0.59 ms at 4K (worst case, every pixel running all 48
gathers: 1.26 ms; ceiling 3 ms). The raster is structural and NOT measurable in the harness: 12 face passes per frame (6 per light, never demoted), each one the production shadow pass over the casters inside 1.5 x
the light radius, 13.55 MiB atlas + 1.13 MiB scratch. The viewer publishes its CPU milliseconds in `mWorldShadowCpuMs` and has Tracy zones `WorldShadow::render/light/face`; the in-world Tracy capture in an
avatar scene (decision D1) is still to be taken.
