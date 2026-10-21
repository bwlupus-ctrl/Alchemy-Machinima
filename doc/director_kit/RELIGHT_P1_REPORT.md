# Relight Phase 1 report

2026-10-20. Implemented on `feature/director-kit-p0` at unchanged HEAD
`01c908fa08f` (the committed revision-2 brief). No commit, push, staging, stash
operation, viewer build or viewer launch was performed. Existing untracked work
and the dirty `vcpkg` submodule were preserved.

## Delivered: Phase 1 only

Sphere-source shading and optional calibrated physical falloff cover both
batched point loops, box points, box/fullscreen projectors, legacy alpha,
legacy material forward lighting, native PBR alpha, and shared Actor FX PBR.
HEAD's independent Actor Mask point batch also carries the same shape array.
Existing light membership, recipient masks, cookie/cone evaluation, shadow
sampling and shadow auction remain in place. No visibility pre-pass, area
shadows, random/temporal sampling, new viewer render target, G-buffer change,
ReShade ABI change, or shared GL-state change was introduced. Phases 2 and 3
were not implemented.

The settings have the exact names and the listed defaults from brief Â§3.4:

| Setting | Default | UI/CPU bounds |
|---|---:|---|
| `RenderLightShapeEnable` | false | master |
| `RenderLocalLightPhysicalFalloff` | false | optional mode |
| `RenderLocalLightDefaultRadius` | 0 m | 0â€“1 m |
| `RenderLocalLightNearFloor` | 0.1 m | 0.0001â€“10 m |
| `RenderLocalLightPhysicalMaxBoost` | 4 | 0â€“100 |

The brief's numeric floor/cap defaults are 0.1 and 4; the switches and world
radius default off/zero. Non-finite setting values fall back to those defaults.
Enable, floor and cap changes rebuild the shader permutation; radius and
physical-mode changes update per-light data without recolouring primitives.

### Data and radius sources (Â§3.1)

`light_shape` / `light_shapes[]` carry `(radius, mode, gain, -1)`. There is no
Phase 2 channel assignment: **w is -1 everywhere**, including forward/alpha.
Fixture radius is evaluated `mCurrentLive[i].mSourceSizeM / 2`, with a sentinel
for legacy rig fixtures to consume the current global default. Each projector
and its bounce omni resolve the same metadata through the manager, independent
of Rig Rim's Include bounce option. The reference distance is the same effective
orbit-radius calculation used by the rig model (including subject/group scale
and the established spatial clamp). World lights use the global radius and
`R/2`, where `R = primitive light radius * 1.5`.

`mirrorFrom()` copies this evaluated source metadata alongside Rig Rim. Manager
lookup includes all live mirror emitters, so copied projectors and bounce omnis
have the source's radius and reference distance. Catchlights, which have no
fixture size field, use the global default and their source rig's effective
orbit reference. The existing clone-only recipient policy and no-shadow policy
are unchanged.

`LLLightState` has a hash-invalidating setter and a zero-radius default. Its
existing snapshots/restores copy the entire state by value, including the new
field. Every existing Rig Rim reset in HW/preview lighting now resets shape.
New reserved uniforms are appended in enum/push_back lockstep before the size
assertion. Batched point arrays remain distinct from the eight-unit forward
array, just as with Rig Rim. Actor Mask point padding uses a zero-radius entry.

### Shading and falloff (Â§3.2â€“3.3)

The shaped branch separates centre-direction diffuse from representative-point
specular. The representative point moves toward the closest point on the
forward reflection ray. PBR keeps the original material alpha in D and applies
only `(alpha / widened_alpha)^2` to specular. A shape-only PBR evaluator guards
the zero half-vector case; the legacy evaluator is untouched. Legacy LUT
materials receive diffuse wrap and a moved highlight, without PBR energy
normalization. The legacy alpha shader continues to have no specular term.

An analytic spherical-cap irradiance integral handles the horizon; a stable
small-angle branch approaches the point-light cosine. Mode 0 substitutes
`E_sphere / (pi * sinÂ²(sigma))` for the cosine while retaining the existing
radial window. Mode 1 uses that same angular ratio times the calibrated,
windowed inverse-square attenuation. Algebraically its diffuse weighting is
`E_sphere * gain * window / (pi * sinÂ²(sigma) * max(dÂ², nearÂ²))` before the cap:
there is one distance term, not sphere irradiance times a second inverse square.
This expresses the unit-radiance integral in the viewer's calibrated point-light
units. At a reference distance, the angular factor is the same in both modes,
so CPU attenuation calibration preserves exposure unless the hard cap binds.
Inside the sphere the integral is clamped to the full hemisphere, with finite
centre vectors and denominators.

Physical attenuation uses the Â§3.3 quartic window, `near=max(radius, NearFloor)`,
and `min(gain * physicalAtten(d), MaxBoost * legacyAtten(0))`. Gain is calculated
on the CPU as `legacyAtten(d_ref) / physicalAtten(d_ref)`, then applied in GLSL.
It never changes the primitive colour/intensity. A reference at/outside the
light's range has no nonzero exposure to match; gain becomes zero. A deliberately
low cap can also prevent identity. Both cases are reported as INCONCLUSIVE by
the per-light calibration diagnostic rather than silently claiming a pass.

Shaped branches execute before the legacy normal-facing early-outs; those
legacy statements stay in their zero-radius branch. Projectors retain their
original radial distance (light centre) separately from their projection-ray
origin. Their authored ambiance, environment reflections, cookies and existing
shadow weighting remain available in the shaped branch.

### Verified Karis constant and reference policy (Â§0)

**The brief's recalled r/(2d) is not the constant in Karis 2013.** The public
[Real Shading in Unreal Engine 4 notes, equation 10, printed page 14](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf)
use `alpha' = saturate(alpha + sourceRadius / (3 * distance))`. Equation 14 on
printed page 16 supplies the normalization ratio. My recollection of the public
representative-point method agrees with keeping D unchanged; direct checking
of the notes resolves the uncertain divisor in favor of **3**. The implementation
and discriminating literal test therefore use **r/(3d)**, not r/(2d). This is the
explicit correction requested by the verification clause, rather than a claim
about any version of shipped UE code. The closest-point vector moves toward the
ray by construction; the printed equation's vector-sign convention is not
blindly copied.

Diffuse uses the public sphere/disk illuminance technique discussed by
Lagarde & de Rousiers, *Moving Frostbite to PBR* (2014), with independent
spherical-cap quadrature as a test oracle. Only these public techniques and the
viewer's own shading/plumbing were used. No proprietary ReLight source or
add-on binary was read, opened, copied, paraphrased or committed.

## Off path, auxiliary views and determinism (Â§6â€“7)

`RELIGHT_SHAPE` is a global shader permutation absent while disabled. It is
injected at shader-file load, **after every per-program clearPermutations()**,
and covered by the existing global-define binary-cache hash. Injection follows
the real `[EXTRA_CODE_HERE]` marker convention; the forward argument macros are
after that marker. Floor and cap literals share that permutation. When the
master is off, every new uniform upload is skipped. The shader's zero-radius
branch contains the existing statements and disabled preprocessing produces the
same token stream as HEAD for all eight modified shader files.

Probe/hero-mirror captures (`gCubeSnapshot`), Prism/VCam feeds, HUDs, impostors
and water reflections receive zero-radius data, preserving their existing
local-light model. No render-state guards, blend changes, colour-mask changes,
draw-buffer changes or targets were needed. Shape maths has no time, frame,
random, history or temporal inputs. Existing authored rig FX are unchanged.

**Full-scene evidence still pending:** the required off-versus-HEAD image diff
and fixed-scene repeated-frame determinism diff cannot be claimed without a
viewer build/launch and controlled scene. Neither was authorized in this task.
The standalone GPU helper determinism check below is narrower evidence and is
not presented as the Â§6 full-frame harness result.

## Director click path and real widgets

**Machinima > Director Consoleâ€¦ (F2) > Light > Rim > Advanced > Light shape.**
The `kc_light_shape` card is in the Advanced section alongside Diagnostics.
Its header Enable switch is `relight_shape_enable`, bound directly to
`RenderLightShapeEnable`. The four rows are `relight_physical`,
`relight_world_size`, `relight_near_floor`, and `relight_max_boost`, each bound
to the corresponding setting. Every control has a short plain-language tooltip;
World light size (m) wraps over two lines rather than eliding. `ALKitCard` creates
the header's real `ALKitSwitch` from `enable_widget`/`enable_setting`, and its
existing enable handling disables dependent content while that switch is off.
The Rim page master dims the document when Rim is off; it does not disable the
independent Light shape switch/card.

The source is `scripts/code_tools/director_kit/director_mockup_data.json`.
`director_kit_gen.py` now pins 2146 rows: four new rows plus a header Enable
switch (header switches are not counted as rows). The generator propagates the
card's source-authored enable tooltip. All three required generators ran after
the C++ edits, followed by all three `--check` modes. Other generated pages have
source-hash changes; their authored controls were not otherwise edited. Tracked
JSON files remain `i/lf w/lf attr/text eol=lf`.

## Re-anchoring against HEAD

The committed brief uses 057e558 references. This table records the actual
pre-edit HEAD locations and the final working-tree anchors. Repeated numbers
identify overloads or declarations; shader paths use their actual HEAD classes.

| File / symbol | HEAD line(s) | Implemented line(s) |
|---|---:|---:|
| `indra/newview/pipeline.cpp` â€” `void LLPipeline::setupHWLights` | 11706 | 11756 |
| `indra/newview/pipeline.cpp` â€” `const auto write_local_light` | 11823 | 11873 |
| `indra/newview/pipeline.cpp` â€” `gDeferredLightProgram.uniform4fv(LLShaderMgr::RIG_RIM_LIGHT` | 27240 | 27297 |
| `indra/newview/pipeline.cpp` â€” `gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::RIG_RIM_LIGHTS` | 27382 | 27444 |
| `indra/newview/pipeline.cpp` â€” `struct ActorPoint` | 27452 | 27515 |
| `indra/newview/pipeline.cpp` â€” `F32 LLPipeline::setupSpotLight` | 28439, 28625 | 28507, 28695 |
| `indra/newview/pipeline.cpp` â€” `shader.uniform4fv(LLShaderMgr::RIG_RIM_LIGHT,` | 28612 | 28680 |
| `indra/newview/alcinelightrigmanager.cpp` â€” `bool ALCineLightRigManager::rigRimParamsFor` | 816 | 845 |
| `indra/newview/alcinelightrigmanager.cpp` â€” `bool ALCineLightRigManager::lightShapeSourceFor` | new | 816 |
| `indra/newview/alcinelightrig.cpp` â€” `void ALCineLightRig::mirrorFrom` | 1037 | 1037 |
| `indra/newview/alcinelightrig.cpp` â€” `render(mCurrentRadius, mCurrentLive, render_globals, mLastFrame)` | 3059 | 3060 |
| `indra/llrender/llrender.cpp` â€” `void LLLightState::setRigRim` | 653 | 662 |
| `indra/llrender/llrender.cpp` â€” `void LLRender::syncLightState` | 901 | 910 |
| `indra/llrender/llrender.cpp` â€” `void LLRender::getLightStateSnapshot` | 1652 | 1665 |
| `indra/llrender/llrender.cpp` â€” `void LLRender::restoreLightUnitContent` | 1661 | 1674 |
| `indra/llrender/llrender.cpp` â€” `void LLRender::restoreLightStateSnapshot` | 1678 | 1691 |
| `indra/llrender/llshadermgr.h` â€” `END_RESERVED_UNIFORMS` | 944 | 946 |
| `indra/llrender/llshadermgr.cpp` â€” `llassert(mReservedUniforms.size() == END_RESERVED_UNIFORMS)` | 2172 | 2186 |
| `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl` â€” `void pbrPunctual` | 794 | 806 |
| `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl` â€” `vec3 pbrCalcPointLightOrSpotLightRimKeepRef` | 1240, 1301 | 1412, 1493 |
| `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl` â€” `void pbrCalcPointLightOrSpotLightRimSplit` | 1324, 1388 | 1516, 1604 |
| `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl` â€” `vec3 pbrCalcPointLightOrSpotLightRimKeep(` | 1409, 1439 | 1625, 1655 |
| `indra/newview/app_settings/shaders/class3/deferred/multiPointLightF.glsl` â€” `for (int light_idx` | 141 | 155 |
| `indra/newview/app_settings/shaders/class3/deferred/multiPointLightF.glsl` â€” `for (int i =` | 178 | 211 |
| `indra/newview/app_settings/shaders/class3/deferred/pointLightF.glsl` â€” `if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))` | 135 | 147, 181 |
| `indra/newview/app_settings/shaders/class3/deferred/spotLightF.glsl` â€” `if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))` | 207 | 249, 316 |
| `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl` â€” `vec3 calcPointLightOrSpotLight` | 136 | 161 |
| `indra/newview/app_settings/shaders/class3/deferred/materialF.glsl` â€” `vec3 calcPointLightOrSpotLight` | 174 | 199 |
| `indra/newview/app_settings/shaders/class2/deferred/pbralphaF.glsl` â€” `vec3 pbrCalcPointLightOrSpotLightRimKeep(` | 191 | 205 |
| `indra/newview/app_settings/shaders/class2/deferred/pbralphaF.glsl` â€” `#define LIGHT_LOOP(i) light +=` | 462 | 476 |
| `indra/newview/app_settings/shaders/class2/deferred/sharedActorFxPbrF.glsl` â€” `vec3 pbrCalcPointLightOrSpotLightRimKeep(` | 236 | 250 |
| `indra/newview/app_settings/shaders/class2/deferred/sharedActorFxPbrF.glsl` â€” `#define LIGHT_LOOP(i) light +=` | 712 | 726 |

## Gates and instrumentation (Â§8)

| Gate | Result / evidence |
|---|---|
| `zs_check.ps1 -Files <one file>` | PASS, separate calls for all eight touched `.cpp` files, including the new test (`-IncludeTests`). `alpanelcinelightrig.cpp` also PASS as an extra rig-header includer. `llrender.cpp`, `llshadermgr.cpp`, the rig/manager TUs and `pipeline.cpp` cover every changed header. Logs `.tmp/relight_zs_*.log`. |
| GLSL validation | PASS: `python scripts/code_tools/relight_shader_check.py`, **52** production shader unit/permutation compilations, master on/off, NVIDIA RTX 5090 / GL 4.6 / driver 610.88. It mirrors the loader's injection position. Includes 1/8/16-light batches, fullscreen spots, Rim auxiliary/emissive carriers, indexed alpha, material alpha/map variants, shared Actor FX opaque/mask/blend and eight-slot replay. Production math helpers also link and execute in a standalone test-only float target. Log `.tmp/relight_shaders.log`. |
| Pure CPU TUT | PASS **8/8**, `alrelightmath.h` / `tests/alrelightmath_test.cpp`, registered in CMake. Covers literal calibration, monotonicity/cutoff, zero-radius near bound, normalization over a hemisphere grid, representative direction/limit/centre, ratio/inside behavior, independent cap quadrature, and forward/deferred attenuation equivalence. Standalone cl/link commands reused the existing test runner/libraries; no viewer target was built. Logs `.tmp/relight_test_{compile,link}.log`, `.tmp/relight_tests.log`. |
| GPU math / determinism | PASS: reference calibration within 1%, cap, wrap and highlight direction; finite inside/antiparallel PBR evaluations. **42** parameter combinations each rendered twice at fixed 1Ã—1 resolution, byte-identical readbacks. Logs use `[RelightQ] ... PASS`. This is a helper test, not a full-scene render comparison. |
| Disabled shader identity | PASS: `python scripts/code_tools/relight_off_check.py` preprocesses the current and HEAD sources with MSVC `/EP` only, compares tokens for each of eight files in base and actor/Rim/parity configurations. Log `.tmp/relight_off.log`. Full-scene OFF image diff remains INCONCLUSIVE pending in-world testing. |
| Director generators | PASS: generation and `--check` for `director_kit_gen.py`, `director_page_sources.py`, `director_functional_manifest.py`. Manifest retains the existing five warnings; registry/currentness check passes. Logs `.tmp/relight_{kit,sources,manifest}*.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS, **all 11 gates**, exit 0. Existing `.tmp/reflect_prebuild_checks.ps1` invokes the installed guard unchanged with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`; it relocates the supported compiler CachePath into the workspace and uses the existing workspace safe-directory config. Log `.tmp/relight_prebuild.log`. No object purge or viewer build. |
| `git diff --check` | PASS; the new untracked source/test/report files were also checked directly for trailing whitespace and conflict markers. |
| JSON line endings | PASS: `git ls-files --eol` for the source JSON, page_sources.json and functional_manifest.json reports LF in index and working tree. |

The installed prebuild guard has compile, UI, maths and generator gates; the
explicit GPU shader gate above is additional. Full viewer shader-program
linkage and scene behavior remain for the normal reviewed build/in-world cycle.
The standalone test setup prints the pre-existing nonfatal `vswhere.exe`
message, but test compilation/linking succeeds.

Production per-light diagnostics use the opt-in `RelightQ` debug-log channel:
`[RelightQ] calibration PASS/FAIL/INCONCLUSIVE`, emitter id, d_ref, and both
attenuations. Phase 2's selected-light counts, K-cap/samples, visibility-pass
resolution/timing, disk-sampling and ranking tests do not apply: none of those
systems was introduced in Phase 1.

## Phase 1 in-world A/B checklist (Â§9)

Run after the separately reviewed viewer build. Keep camera, resolution,
exposure, environment, pose, material settings and rig FX fixed between captures.
Use a wall and floor, an avatar 0.5 m from the wall, one fixture-mode rig key
with source size 0.6 m, and one world point light behind the wall.

- [ ] Capture unchanged HEAD and this implementation with Enable OFF; pixel-diff
  them at fixed resolution. Require zero difference. Also toggle onâ†’off and
  compare, checking that stale shape arrays cannot survive shader reload.
- [ ] Enable shape with Physical falloff OFF. Sweep source size from 0.01 m to
  0.6 m and larger. Skin/PBR highlights should broaden; roughness stays authored;
  wrap survives the terminator. The smallest source should approach today's
  appearance. Repeat on legacy glossy materials (moved highlight + diffuse wrap).
- [ ] Turn Physical falloff ON at the same size. At the rig effective orbit
  reference, brightness should agree within 1% while the cap is inactive. The
  farther wall should darken and close objects brighten. Enable the RelightQ
  debug channel and check every rig emitter's calibration verdict. Repeat with
  a world source radius >0; its reference is R/2.
- [ ] Move a small light within 5 cm of the face and through the surface/source
  centre. Sweep Near floor and Max boost. Require bounded, finite output with no
  flashing/NaNs; a binding cap may intentionally prevent reference identity.
- [ ] Check alpha hair and sheer cloth against the opaque body under both
  falloff modes, legacy alpha/material and PBR alpha, including shared Actor FX.
- [ ] Move the camera across the point-light volume boundary to switch box and
  fullscreen batches, and test fullscreen/projector paths with cookies, gobo,
  ambiance and legacy environment reflections. Require continuous appearance.
- [ ] Test bounce on/off and mirrored Reflect/True Mirror clone rigs. The copied
  fixture/source radius and look should track edits; copied lights remain
  clone-only, and neither consume nor displace shadow slots. Test Actor Mask,
  its F3 debug bypass, Rig Rim normal/debug/silhouette, and forward slot changes.
- [ ] Test world default radius 0 (exact point branch), then 0â€“1 m. With shape
  off, changing any dependent saved setting must not affect the image.
- [ ] Confirm probe/hero mirror captures, Prism/VCam, HUD, impostor and water
  reflection retain their prior lighting. Check the ReShade bridge as before.
- [ ] Render the same fixed frame twice with shape on and pixel-diff the entire
  output; require bit identity, recording `[RelightQ] determinism PASS/FAIL`.
- [ ] For the user's visual reference A/B, first disable the in-engine feature
  and capture the user-authored ReShade light setup, then disable that effect
  and capture the engine version at similar light positions. Compare Phase 1
  highlight shape, wrap and falloff only. **Wall leakage and contact-hardening
  shadows are not Phase 1 acceptance criteria and are expected to remain as
  before.** Do not enable both lighting effects together for this comparison.


## Review fixes

2026-10-20 fix round against `.tmp/review_codex_relight.md`. These notes
supersede the earlier floor/cap permutation and validation descriptions above.

- **Horizon:** shaped legacy and PBR evaluators reject a sphere wholly below
  the surface horizon before evaluating either lighting term. Legacy specular
  requires a positive representative N.L; epsilon protection applies only to
  denominators. The shaped PBR evaluator likewise keeps zero N.L separate from
  its protected denominator. Checked the shared helper callers in deferred
  point/spot/both multi loops, forward material/PBR alpha and Actor FX PBR;
  legacy alpha uses the sphere diffuse ratio without a specular term.
- **Disabled hash:** all five `setLightShape` call sites, including preview/HW
  resets, are gated directly by the cached master setting. Disabling does not
  clear stale shape state or invalidate the light hash for shape reasons.
  Disabled uploads remain skipped and the compiled shader uses the existing
  point path. Enabling sets current shapes again, including auxiliary-view
  zero-radius sentinels.
- **Live settings:** removed Near floor and Max boost shader-change listeners
  and compile literals. They are sanitized live `relight_limits` uniforms,
  uploaded through the existing full/fast/forward bind hook even when Rig Rim
  is off, and skipped when Light shape is off. CPU calibration reads the live
  floor at light upload. No shape sub-setting invokes shader reload; only the
  master retains the required permutation reload.
- **Forward diffuse:** restored the [0,1] clamp on the complete shaped diffuse
  weight in alpha and material lighting, preserving the mode-0 small-radius
  limit near a light with zero falloff.
- **Link/runtime coverage:** the shader gate now links production vertex and
  fragment programs with production utility modules, indexed lookup code,
  skinning and shadow defines. Coverage includes box/fullscreen point/spot,
  1/8/16-light batches, Actor Mask wrappers, all legacy material alpha/map
  combinations, native PBR alpha and scalar/eight-slot Actor FX PBR replay.
  GPU tests use a nonzero gloss LUT and require exact zero diffuse/specular
  for fully hidden spheres in both falloff modes, with a positive control and
  a horizon-crossing diffuse-only case. CPU checks include hidden spheres and
  the small-source forward clamp. Live floor/cap updates are tested on the
  same linked GPU program without recompilation.

Validation after fixes:

| Gate | Result / evidence |
|---|---|
| Separate `zs_check` calls | PASS, all nine touched C++ TUs, one file per call; `.tmp/relight_review_zs_*.log`. |
| Shader validation/link/readback | PASS. See final run `.tmp/relight_review_shader_links.log`; 56 isolated unit/permutation compilations plus 84 production program links, master on/off; 84 deterministic repeated legacy/PBR helper readbacks at fixed 1x1. |
| `alrelightmath` | PASS 8/8 after recompiling/relinking only the standalone test; `.tmp/relight_review_tests.log`. |
| Disabled GLSL identity | PASS, all eight modified files versus HEAD; `.tmp/relight_review_off.log`. |
| Kit generators | Regenerated all three outputs after the last C++ edits; all three `--check` modes PASS. `.tmp/relight_review_{kit,sources,manifest}*.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`, via the existing workspace wrapper; `.tmp/relight_review_prebuild.log`. |
| `git diff --check` | PASS. |

No commit, push, stash, viewer build or viewer launch. Full-scene image and
frame-determinism evidence remains pending the authorized in-world review;
the helper readbacks do not replace it.


## Runtime toggle fix

2026-10-20, against committed Phase 1 HEAD `e0b4c27ddb2`. This section
supersedes all earlier descriptions of a master permutation/reload.

- Removed the `RenderLightShapeEnable` shader-change listener, the global
  `RELIGHT_SHAPE` define, and its loader injection. None of the five Relight
  settings has a reload listener or shader-manager dependency. Toggling the
  master changes uniforms/upload gates only; it calls neither `setShaders()`
  nor a GL resource rebuild.
- The same eight affected GLSL modules now compile their shaped code
  unconditionally. Added reserved `RELIGHT_ENABLED` / `relight_enabled` in
  enum/name lockstep. Every shaped entry tests the master before source radius;
  projector legacy early-outs also run when the master is off. No other shader
  source was edited. All legacy and shaped expressions match HEAD after
  removing only the new uniform declaration and master predicates.
- The existing full/fast/forward bind hook always refreshes the master uniform,
  including zero while disabled and with Rig Rim off. Stale nonzero GPU shape
  arrays are ignored. Shape arrays, per-light setters and limits uploads remain
  skipped while disabled. Disabling neither clears stored shape state nor
  changes the light hash for shape reasons. Enabling bumps the forward light
  hash once so programs cached during the disabled interval repopulate shape
  arrays even if their light records did not otherwise change. Auxiliary-view
  zero-radius policy is unchanged.
- Physical falloff and world size remain live per-light data; Near floor and
  Max boost remain live uniforms/CPU calibration inputs. The source gate checks
  all five names for reload listeners and shader-manager dependencies. GPU
  checks vary both falloff modes and change limits on the same linked program.
- Both known P1s in `materialF.glsl` remain untouched: the shaped legacy
  specular weighting/clamp issue and the Actor FX glare suppression issue.
  Whole-expression token comparison also protects these existing expressions.

Listener audit (report only; no unrelated changes):

- `RenderSMAAPredicationThreshold`, `RenderSMAAPredicationScale`, and
  `RenderSMAAPredicationStrength` were added in April 2026 (`e2d37faecfb9`).
  Their listeners trigger full reloads, although the three constants are only
  arithmetic inputs in `SMAACalculatePredicatedThreshold` (`SMAA.glsl:710-711`)
  and could be uploaded as uniforms. The predication master separately selects
  a compiled feature; it was not changed.
- Older scalar candidate: `RenderTerrainPBRTriplanarBlendFactor` (2023) becomes
  a global compile literal, then a local `sharpness` arithmetic value in
  `pbrterrainUtilF.glsl:251`; a uniform could replace that numeric literal.
  `RenderGammaFull` is an older (2022) reload listener with no remaining C++ or
  GLSL reader found beyond its setting declaration/listener.
- Recent `RenderGBufferAvatarTag`, weather rain occlusion/lightning quality,
  projector Dust/ConservativeShadow, and `RenderVisibleDiffuseSidecar` listeners
  currently select actual shader programs/permutations or render attachments;
  they are not simply live scalar uploads. All were left unchanged.

Validation:

| Gate | Result / evidence |
|---|---|
| Shader validation + `relight_shader_check.py` | PASS: 28 isolated unit/permutation compilations and 42 production program links, with no compile-time master define. Covers the same program variants as the earlier 56/84 run, once each because on/off now share an executable. `.tmp/relight_runtime_shaders_final.log`. |
| Runtime GPU toggle | PASS: eight production forward evaluator cases across both falloff modes, linked with production utility modules; on/off/on without relinking, stale nonzero shape data, exact legacy zero-radius equality, disabled limits unchanged, and restored shaped output. Same log. |
| Existing GPU math/readback | PASS: live floor/boost, horizon rejection, calibration, finite math, and 84 repeated deterministic helper readbacks. Same log. |
| `relight_off_check.py` | PASS: all eight files' full legacy/shaped expression token streams match HEAD after accounting only for the new master; all five settings have no reload listener or shader-manager dependency. `.tmp/relight_runtime_identity.log`. |
| `alrelightmath` | PASS 8/8; compiled/relinked standalone test only. `.tmp/relight_runtime_math{_compile,_link,}.log`. |
| Separate `zs_check` calls | PASS: all six touched C++ TUs, one file per invocation. `.tmp/relight_runtime_zs_{llrender,llshadermgr,alcinerigrim,llviewercontrol,llviewershadermgr,pipeline}.log`. |
| Kit outputs | Regenerated all three generators after the C++ edits; no tracked output differences. All three check modes PASS. `.tmp/relight_runtime_{kit,sources,manifest}*.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, using the existing workspace wrapper with `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. `.tmp/relight_runtime_prebuild.log`. |
| `git diff --check` | PASS. |

No commit, push, staging, stash, viewer build or viewer launch. Existing unrelated
work is preserved. Full-scene visual identity and in-world responsiveness still
require viewer verification; the source/GPU gates do not claim an in-world run.


## In-world report: less face, more rim/highlights

2026-10-20, diagnosis and fix against committed Phase 1 `81be37dd6d1`.
This section supersedes the earlier notes leaving the two materialF P1s open.

**Measured result:** Light shape did not reduce front-facing diffuse. The
fully visible sphere ratio already had the correct `sin^2(sigma)` and pi
normalization, centre light direction and single legacy distance window.
The measured deviations were increased legacy highlight energy, missing
spot/material specular clamps, silhouette glare leakage, and Rig Rim changing
when physical falloff was selected. The default-mode Rig Rim term was unchanged.
The user's whole-scene perception of "less face" is not reproduced as a direct
diffuse loss by these measurements. Broadening a highlight can brighten its
tails while lowering its peak; diffuse wrap can legitimately increase edge light.
No exposure gain or diffuse brightness compensation was added.

Method and reproduction:

- `scripts/code_tools/relight_shader_check.py` now executes
  `relight_rig_check.py` in its WGL context. Actual deferred main bodies and
  forward per-light functions link against production utility modules. Only
  G-buffer/surface inputs and light uniforms are controlled fixtures; there is
  no replacement CPU lighting evaluator or stubbed lighting function.
- Deferred: multiPoint PBR/legacy loops, point PBR/legacy and spot PBR/legacy.
  Forward: alphaF (diffuse only), materialF, pbralpha's reference and split
  evaluators, and shared Actor FX PBR's split evaluator. Box/fullscreen,
  shadow, skin, material/map and Actor Mask permutations also retain the full
  production-program compilation/link gate.
- Source distances 1 and 2 m; source **radii** .05/.3/.6 m (fixture Source size
  .1/.6/1.2 m); shader range 15 m; legacy falloff .5; white light; base colour
  .6. Camera-facing reference normal/view +Z, surface position (0,0,-4),
  light direction +Z; normals rotate in XZ through 0/30/60/85/95 degrees.
  Thus the camera remains frontal to the reference face while its surface
  normals span a curved face. PBR roughness .3/.6, metallic 0; legacy gloss
  .25/.75, specular sRGB .5. The real default 1024x256 gloss LUT is reproduced
  from `pipeline.cpp:3427`, exponent 368, R16F, clamped addressing and the
  production filter settings. Diffuse and specular readbacks are isolated
  through material inputs/spec-keep, and Rim uses the production rim-only mode
  with nonzero positive controls.
- Peak = maximum of **each** lobe independently over a -89..89 degree normal
  sweep at .5 degree intervals. Width is measured at half of each lobe's own
  peak. A pointwise ON/OFF specular ratio in a tail is not a peak comparison.
  There are 126 distance/radius/material/path rows, each with five diffuse,
  specular and Rim pairs plus the dense peak sweep. Additional probes cover
  tiny-radius grazing, high-intensity clamp, silhouette glare and physical Rim.
- Before: `python scripts/code_tools/relight_shader_check.py --rig-only
  --shader-ref 81be37dd6d1 --rig-measure-only --rig-json .tmp/relight_rig_before.json`.
  This reads committed shader sources directly with `git show`; it does not
  check out, stash or alter the worktree. Before has **68** regression failures.
- After/full gate: `python scripts/code_tools/relight_shader_check.py
  --rig-json .tmp/relight_rig_after.json`. **Zero** regression failures.
  Raw per-angle values, undefined ratios, peaks, widths and stress readbacks:
  [before JSON](../../.tmp/relight_rig_before.json),
  [after JSON](../../.tmp/relight_rig_after.json). Measure-only explicitly records
  failures; the normal gate asserts them. `--rig-only` skips the large compile
  permutation sweep for diagnostic iteration; it was not used for the final gate.

Diffuse ON/OFF ratios, physical falloff OFF (all applicable caller/material
variants agree to float roundoff; displayed precision 6 decimals). Each cell
is **before -> after**. The 95-degree column is separated below because a true
point source there contributes zero.

| Distance m | Radius m | 0 degrees | 30 degrees | 60 degrees | 85 degrees |
|---|---|---|---|---|---|
| 1 | 0.05 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 |
| 1 | 0.3 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.338981 -> 1.338981 |
| 1 | 0.6 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.005424 -> 1.005424 | 2.195716 -> 2.195716 |
| 2 | 0.05 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 |
| 2 | 0.3 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.045680 -> 1.045680 |
| 2 | 0.6 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.000000 -> 1.000000 | 1.338981 -> 1.338981 |

At 95 degrees, multiPoint legacy and forward alpha/material have an exactly
zero OFF denominator; their ON/OFF ratio is undefined (ON also zero for the
.05 m sphere, otherwise positive wrap). Point/spot legacy retain their existing
`1e-6` point N.L floor; PBR retains its `.001` floor. The following are the
**actual production** ratios for those paths, not ratios against ideal zero.
These epsilon remnants are left byte-identical when OFF. No normalization bug
should be inferred from a large ratio against an epsilon denominator.

| Distance m | Radius m | Point/spot legacy 95-degree ratio, before -> after | PBR 95-degree ratio, before -> after |
|---|---|---|---|
| 1 | 0.05 | 0.000000 -> 0.000000 | 0.000000 -> 0.000000 |
| 1 | 0.3 | 29544.087891 -> 29544.087891 | 29.544085 -> 29.544085 |
| 1 | 0.6 | 104213.531250 -> 104213.531250 | 104.213516 -> 104.213516 |
| 2 | 0.05 | 0.000000 -> 0.000000 | 0.000000 -> 0.000000 |
| 2 | 0.3 | 3981.202148 -> 3981.202148 | 3.981202 -> 3.981202 |
| 2 | 0.6 | 29544.087891 -> 29544.087891 | 29.544085 -> 29.544085 |

Fully visible sources now return N.L directly from `relightRatio`, eliminating
the needless `pi*s2` multiply/divide roundoff. CPU equality is exact; production
caller comparisons allow normalization/forward attenuation float roundoff
(relative tolerance 2e-6). This rounding cleanup does not explain a visible
brightness change. Only horizon-crossing samples gain diffuse.

Specular **peak** ON/OFF, before -> after. The PBR numbers cover deferred
multi/point/spot and both forward/native/Actor FX PBR evaluators. Legacy .25
numbers cover all specular legacy paths; .75 is split where the original
spot/material clamp changes the baseline peak. AlphaF has no local specular.

| Distance m | Radius m | PBR rough .3 | PBR rough .6 | Legacy gloss .25 | Legacy gloss .75 multi/point | Legacy gloss .75 spot/material |
|---|---|---|---|---|---|---|
| 1 | 0.05 | 0.712022 -> 0.712022 | 0.913722 -> 0.913722 | 1.000254 -> 1.000000 | 1.000254 -> 1.000000 | 1.409119 -> 1.000000 |
| 1 | 0.3 | 0.226848 -> 0.226848 | 0.618978 -> 0.618978 | 1.011106 -> 1.000000 | 1.011106 -> 1.000000 | 1.424406 -> 1.000000 |
| 1 | 0.6 | 0.101491 -> 0.101491 | 0.433385 -> 0.433385 | 1.053294 -> 1.000000 | 1.053294 -> 1.000000 | 1.483840 -> 1.000000 |
| 2 | 0.05 | 0.837697 -> 0.837697 | 0.955293 -> 0.955293 | 1.000038 -> 1.000000 | 1.000038 -> 1.000000 | 1.214743 -> 1.000000 |
| 2 | 0.3 | 0.414266 -> 0.414266 | 0.772957 -> 0.772957 | 1.002442 -> 1.000000 | 1.002442 -> 1.000000 | 1.217662 -> 1.000000 |
| 2 | 0.6 | 0.226848 -> 0.226848 | 0.618978 -> 0.618978 | 1.011106 -> 1.000000 | 1.011106 -> 1.000000 | 1.228187 -> 1.000000 |

The original PBR code already keeps alpha in D and multiplies only by
`(alpha/alphaPrime)^2` (`deferredUtil.glsl:989`); it never exceeded the point
peak in this matrix and is unchanged. Tail increases remain legitimate.
Legacy now keeps its original centre-based angular, Fresnel and geometry
weights and authored LUT amplitude; only the LUT highlight-position coordinate
uses the representative half vector. It receives no alpha normalization.
This preserves legacy peak amplitude; it does not assert constant integrated
energy for an unnormalized broadened LUT lobe.

Example angular widths (FWHM degrees, OFF / ON), at 1 m and .6 m source radius:

| Material, deferred point | Before OFF / ON | After OFF / ON |
|---|---|---|
| PBR roughness .3 | 6.5 / 43.5 | 6.5 / 43.5 |
| PBR roughness .6 | 29.5 / 62.5 | 29.5 / 62.5 |
| Legacy gloss .25 | 28.5 / 58.5 | 28.5 / 60.5 |
| Legacy gloss .75 | 10.5 / 45.5 | 10.5 / 46.5 |

Rig Rim: mode-0 ON/OFF is 1 before and after at every nonzero Rim sample
in all paths/materials; zero samples remain zero. Neither diffuse wrap nor
the representative vector is passed into Rim. With physical falloff ON, the
old shaped attenuation did change Rim. Extra probe: distance 1.5 m, radius
.3 m, normal 85 degrees, gain 1, same white cookie/no shadow:

| Caller | Rim before OFF / ON | Rim after OFF / ON |
|---|---|---|
| multiPoint legacy | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| multiPoint PBR | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| point legacy | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| point PBR | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| spot legacy | 0.234848544 / 0.144939229 | 0.234848544 / 0.234848544 |
| spot PBR | 0.234848544 / 0.144939229 | 0.234848544 / 0.234848544 |
| alpha legacy | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| material legacy | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| pbralpha PBR | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| pbrsplit PBR | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |
| actorPbr PBR | 0.234848544 / 0.144939259 | 0.234848544 / 0.234848544 |

Known P1 discriminating probes (raw linear output, not peak sweeps):

| Probe | Before OFF / ON | After OFF / ON |
|---|---|---|
| material tiny radius 1e-6 m, gloss .05, 85 degrees: specular | 0.000619356 / 0.003716138 | 0.000619356 / 0.000619356 |
| material tiny radius, sil_keep=0: glare alpha | 0.000000000 / 0.057174247 | 0.000000000 / 0.000000000 |
| material tiny radius, sil_keep=.25: glare alpha | 0.028587125 / 0.071467809 | 0.028587125 / 0.028587125 |
| material power 8, falloff 0, gloss .75: per-light specular clamp | 1.000000000 / 25.357692719 | 1.000000000 / 1.000000000 |

Root causes and minimal fixes (line references are current; parenthesized
references identify the committed faulty expressions):

- **Legacy representative weighting boosted the peak.**
  `class1/deferred/deferredUtil.glsl:1009-1022` (HEAD `1006-1016`) used the
  representative direction for N.L and the entire Fresnel/geometry factor.
  When the representative ray reaches reflection alignment away from normal
  incidence, that changes the peak weight (up to 1.053294 in the requested
  matrix). Only the LUT lookup now uses the representative half vector;
  centre-based weights remain the point-light weights. Removed the shaped
  `max(nh*nl,1e-5)` weighting floor after positive nh/nl tests, so it cannot
  darken the tiny-radius grazing limit independently of the legacy formula.
- **Forward material borrowed deferred specular weighting.**
  `class3/deferred/materialF.glsl:211-225` (HEAD `210-213`) called
  `relightLegacy`, which uses `min(N.L*6,1)`; forward instead uses the original
  `lit=clamp(spot^2*N.L*attenuation,0,1)` with geometry based on its `da`.
  The old tiny-source 85-degree specular was 6x its point result. The shaped
  branch now evaluates the forward weights locally, moving only the LUT
  coordinate and retaining cone dependence. It restores the complete per-light
  colour clamp [0,1]. No changes to the point/OFF branch.
- **Projector legacy also lost its per-light specular clamp.**
  `class3/deferred/spotLightF.glsl:256-263` (HEAD `256-257`) multiplied an
  unclamped combined legacy helper result by lamp/shadow, unlike the original
  projector branch. Shaped diffuse and highlight now compose separately;
  the coloured/shadowed highlight receives the same [0,1] clamp. Together
  with representative weighting this produced 1.483840 peak ratios at gloss
  .75/radius .6/distance 1 before the fix.
- **Actor FX sil_keep missed part of the glare update.**
  `class3/deferred/materialF.glsl:227-231` (HEAD `215-224`) scaled only the
  added max-channel glare, leaving `max(glare,speccol.r)` unsuppressed. Shaped
  glare now snapshots the incoming value, performs the original complete
  update and scales its entire delta by sil_keep, exactly as the OFF branch.
  Zero keep now emits zero additional glare; partial keep scales the whole delta.
- **Rig Rim received physical attenuation.**
  `class3/deferred/multiPointLightF.glsl:167,222`,
  `class3/deferred/pointLightF.glsl:143`,
  `class3/deferred/spotLightF.glsl:269`,
  `class2/deferred/alphaF.glsl:178`,
  `class3/deferred/materialF.glsl:238`, and both forward PBR implementations in
  `class1/deferred/deferredUtil.glsl:1435,1541` fed `relightAtten` into Rim.
  Mode 0 happened to match; mode 1 did not. Rim now always uses the original
  legacy distance attenuation, original centre direction and existing cone,
  cookie, shadow, material/quality inputs. Direct shaped lighting retains its
  selected falloff. The scalar and split PBR evaluators were both fixed.
- **No diffuse energy bug found.** `deferredUtil.glsl:935-955` uses the correct
  analytical sphere integral and `pi*sin^2(sigma)` ratio. `relightPbr` evaluates
  diffuse with centre direction and specular with the representative direction;
  no caller feeds a representative direction into diffuse or Rim. The added
  fully-visible direct-N.L return and matching CPU helper eliminate only
  rounding, while the existing horizon/inside handling remains intact.

Validation and limits:

| Gate | Result / evidence |
|---|---|
| Full `relight_shader_check.py` | PASS: 28 isolated shader/permutation compilations, 42 production program links, plus 8 rig-fixture programs, 126 matrix rows, peak/width scans, positive controls and stress tests. NVIDIA RTX 5090 / OpenGL 4.6 / driver 610.88; `.tmp/relight_rig_shaders_final.log`. |
| Runtime master and determinism | PASS: original 8 on/off/on production forward probes, stale shape storage and disabled live limits, 84 repeated deterministic helper readbacks; same linked executables. No settings/listener/plumbing changes. |
| OFF identity | PASS: **1890** recorded OFF component readbacks exactly equal the committed baseline (float32 byte identity); full disabled-expression token checks for all 8 shader modules and five-setting no-reload audit, `.tmp/relight_rig_off.log`. The source guard now compares the disabled branches, allowing the intentional ON fixes. |
| `alrelightmath` | PASS 9/9. Added the requested rig geometry matrix, exact fully-visible N.L equality, horizon-only wrap and nonboosting Karis factors. Standalone test compiled/relinked with existing runner/libraries; `.tmp/relight_rig_math{_compile,_link,}.log`. |
| `zs_check` touched C++ | PASS: `tests/alrelightmath_test.cpp`, one TU in its own call; `.tmp/relight_rig_zs.log`. This is the only C++ TU changed for this task; it also covers the changed header. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS all 11 gates, via the existing workspace wrapper; `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. `.tmp/relight_rig_prebuild.log`. |
| `git diff --check` | PASS. |

No commit, push, staging, stash, viewer build or viewer launch. Unrelated
concurrent work was preserved. Shader readbacks verify these shading contracts;
the user's full in-world image, tone mapping/post effects and visual response
still need the normal reviewed viewer build and A/B. The measurements do not
claim a full-scene screenshot test or an independent Opus review.
