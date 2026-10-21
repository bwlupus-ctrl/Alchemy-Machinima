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
`FeedPasses` admits at most eight actual shadow passes per frame, including
multiple renders of the same Gate source. The ninth skips before allocation
or gather, with invalid channels; a new frame resets only this work counter.
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
